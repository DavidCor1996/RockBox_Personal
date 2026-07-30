"""Atomic Maker Lite export, cover conversion, and launcher manifests."""

from __future__ import annotations

import hashlib
import os
import shutil
import struct
import tempfile
import zlib
from pathlib import Path

from services.maker_lite_pack import compile_project
from services.maker_lite_settings import compile_device_settings, load_settings

try:
    from PIL import Image, ImageColor
except ImportError:  # pragma: no cover
    Image = None
    ImageColor = None


class MakerLiteExportError(ValueError):
    pass


DEVICE_ROOT = ".rockbox/games/maker_lite"
PLUGIN_PATH = "/.rockbox/rocks/games/maker_lite.rock"
MANIFEST_PATH = ".rockbox/rocks/games/maker_lite/games.tsv"
BROWSER_MANIFEST_PATH = ".rockbox/games/maker_lite/projects.tsv"


def private_root_for_config(config) -> str:
    return os.path.join(os.path.dirname(config.db_path), "maker_lite")


def render_cover(source_path: str, destination: str, fit: str = "contain",
                 background: str = "#000000") -> str:
    if Image is None:
        raise MakerLiteExportError("Pillow is required for Maker Lite covers")
    if fit not in {"contain", "crop"}:
        raise MakerLiteExportError("Cover fit must be contain or crop")
    with Image.open(source_path) as opened:
        source = opened.convert("RGB")
    target_size = (144, 108)
    if fit == "crop":
        scale = max(target_size[0] / source.width, target_size[1] / source.height)
    else:
        scale = min(target_size[0] / source.width, target_size[1] / source.height)
    size = (max(1, round(source.width * scale)), max(1, round(source.height * scale)))
    resized = source.resize(size, Image.Resampling.LANCZOS)
    color = ImageColor.getrgb(background)
    canvas = Image.new("RGB", target_size, color)
    offset = ((target_size[0] - size[0]) // 2, (target_size[1] - size[1]) // 2)
    canvas.paste(resized, offset)
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    temporary = destination + ".tmp.bmp"
    canvas.save(temporary, format="BMP")
    os.replace(temporary, destination)
    return hashlib.sha256(Path(destination).read_bytes()).hexdigest()


def export_project(record, private_root: str) -> dict:
    source = record.source
    project_id = source["project_id"]
    output = os.path.join(os.path.abspath(private_root), "exports", project_id)
    os.makedirs(output, exist_ok=True)
    pack_path = os.path.join(output, "game.mlp")
    temporary = pack_path + ".tmp"
    with open(temporary, "wb") as compiled:
        compiled.write(compile_project(source))
        compiled.flush()
        os.fsync(compiled.fileno())
    os.replace(temporary, pack_path)

    cover_source = str(source.get("metadata", {}).get("cover_source", ""))
    cover_path = ""
    if cover_source:
        if not os.path.isfile(cover_source):
            raise MakerLiteExportError("Selected cover is no longer readable")
        cover_path = os.path.join(output, "cover.144x108x24.bmp")
        render_cover(
            cover_source,
            cover_path,
            source.get("metadata", {}).get("cover_fit", "contain"),
            source.get("metadata", {}).get("cover_background", "#000000"),
        )
    return {"directory": output, "pack": pack_path, "cover": cover_path}


def _copy_atomic(source: str, destination: str) -> None:
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".maker-lite-", dir=os.path.dirname(destination))
    os.close(fd)
    try:
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def _write_atomic(data: bytes, destination: str) -> None:
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    fd, temporary = tempfile.mkstemp(
        prefix=".maker-lite-", dir=os.path.dirname(destination)
    )
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, destination)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def _stage_file(destination: str, source: str = "", data: bytes | None = None) -> str:
    """Write a complete sibling temporary without changing the live target."""

    os.makedirs(os.path.dirname(destination), exist_ok=True)
    fd, temporary = tempfile.mkstemp(
        prefix=".maker-lite-stage-", dir=os.path.dirname(destination)
    )
    try:
        if data is None:
            os.close(fd)
            shutil.copy2(source, temporary)
            with open(temporary, "rb") as staged:
                os.fsync(staged.fileno())
        else:
            with os.fdopen(fd, "wb") as output:
                output.write(data)
                output.flush()
                os.fsync(output.fileno())
        return temporary
    except Exception:
        try:
            os.close(fd)
        except OSError:
            pass
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def _commit_staged(staged: list[tuple[str, str]]) -> None:
    """Commit a staged file set and restore prior targets if a rename fails."""

    committed: list[tuple[str, str]] = []
    active_backup = ""
    active_destination = ""
    try:
        for temporary, destination in staged:
            active_destination = destination
            active_backup = ""
            if os.path.lexists(destination):
                if os.path.isdir(destination):
                    raise MakerLiteExportError(
                        f"Device target is unexpectedly a directory: {destination}"
                    )
                fd, active_backup = tempfile.mkstemp(
                    prefix=".maker-lite-backup-",
                    dir=os.path.dirname(destination),
                )
                os.close(fd)
                os.unlink(active_backup)
                os.replace(destination, active_backup)
            os.replace(temporary, destination)
            committed.append((destination, active_backup))
            active_destination = ""
            active_backup = ""
    except Exception:
        if active_backup:
            try:
                if os.path.lexists(active_destination):
                    os.unlink(active_destination)
                os.replace(active_backup, active_destination)
            except OSError:
                pass
        for destination, backup in reversed(committed):
            try:
                if os.path.lexists(destination):
                    os.unlink(destination)
                if backup:
                    os.replace(backup, destination)
            except OSError:
                pass
        raise
    finally:
        for temporary, _destination in staged:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass
        for _destination, backup in committed:
            if backup:
                try:
                    os.unlink(backup)
                except FileNotFoundError:
                    pass


def _validate_private_art(path: str, kit_id: str) -> None:
    try:
        data = Path(path).read_bytes()
        version, cell_size, cell_count, player_base, expected_crc = (
            struct.unpack_from("<HHHHI", data, 4)
        )
    except (OSError, struct.error) as exc:
        raise MakerLiteExportError(f"Private kit is unreadable: {kit_id}") from exc
    encoded_id = data[16:48].split(b"\0", 1)[0]
    content = data[64:]
    pixel_bytes = cell_count * 16 * 16 * 2
    table_entries = 14 if version == 2 else 56 if version in {3, 4} else 0
    table_bytes = table_entries * 4
    frame_count = 0
    expected_size = pixel_bytes + table_bytes
    if (
        len(data) < 64
        or data[:4] != b"MLAR"
        or version not in {1, 2, 3, 4}
        or cell_size != 16
        or not 1 <= cell_count <= 1536
        or encoded_id != kit_id.encode("ascii")
        or zlib.crc32(content) & 0xFFFFFFFF != expected_crc
    ):
        raise MakerLiteExportError(f"Private kit failed validation: {kit_id}")
    if version == 4:
        frame_header = pixel_bytes + table_bytes
        try:
            frame_count, reserved = struct.unpack_from(
                "<HH", content, frame_header
            )
        except struct.error as exc:
            raise MakerLiteExportError(
                f"Private kit failed metasprite validation: {kit_id}"
            ) from exc
        expected_size = frame_header + 4 + frame_count * 36
        if (
            reserved != 0
            or not 14 <= frame_count <= 512
            or player_base + 13 >= frame_count
            or len(content) != expected_size
        ):
            raise MakerLiteExportError(
                f"Private kit failed metasprite validation: {kit_id}"
            )
        for frame_index in range(frame_count):
            offset = frame_header + 4 + frame_index * 36
            columns, rows, offset_x, offset_y, *cells = struct.unpack_from(
                "<BBbb16H", content, offset
            )
            used = columns * rows
            if (
                not 1 <= columns <= 4
                or not 1 <= rows <= 4
                or not -64 <= offset_x <= 64
                or not -64 <= offset_y <= 64
                or any(
                    cell != 0xFFFF and cell >= cell_count
                    for cell in cells[:used]
                )
                or any(cell != 0xFFFF for cell in cells[used:])
            ):
                raise MakerLiteExportError(
                    f"Private kit failed metasprite validation: {kit_id}"
                )
    elif player_base + 13 >= cell_count or len(content) != expected_size:
        raise MakerLiteExportError(f"Private kit failed validation: {kit_id}")
    if version in {2, 3, 4}:
        animation_limit = frame_count if version == 4 else cell_count
        for index in range(table_entries):
            start, count, ticks = struct.unpack_from(
                "<HBB", content, pixel_bytes + index * 4
            )
            ticks &= 0x7F
            if (
                count == 0
                or ticks == 0
                or ticks > 60
                or start >= animation_limit
                or count > animation_limit - start
            ):
                raise MakerLiteExportError(
                    f"Private kit failed animation validation: {kit_id}"
                )


def sync_projects(
    records,
    private_root: str,
    mount_root: str,
    plugin_source: str = "",
) -> dict:
    """Sync packs, shared kits, covers, and both authoritative indexes."""

    mount_root = os.path.abspath(mount_root)
    rows = []
    browser_rows = []
    operations: list[tuple[str, str, bytes | None]] = []
    copied = []
    copied_kits = set()
    for record in records:
        exported = export_project(record, private_root)
        source = record.source
        project_id = source["project_id"]
        project_device_dir = os.path.join(mount_root, DEVICE_ROOT, "projects", project_id)
        pack_destination = os.path.join(project_device_dir, "game.mlp")
        operations.append((pack_destination, exported["pack"], None))
        copied.append(pack_destination)
        settings_destination = os.path.join(
            mount_root, DEVICE_ROOT, "settings", project_id + ".mlc"
        )
        operations.append(
            (
                settings_destination,
                "",
                compile_device_settings(
                    project_id, load_settings(private_root, project_id)
                ),
            )
        )
        copied.append(settings_destination)

        kit_id = source["kit_id"]
        kit_source = os.path.join(private_root, "kits", kit_id)
        art_source = os.path.join(kit_source, "art.mla")
        if not os.path.isfile(art_source):
            raise MakerLiteExportError(f"Private kit is missing: {kit_id}")
        _validate_private_art(art_source, kit_id)
        if kit_id not in copied_kits:
            kit_destination = os.path.join(mount_root, DEVICE_ROOT, "kits", kit_id)
            for name in ("art.mla", "audio.mla", "kit.mlk", "provenance.tsv"):
                source_file = os.path.join(kit_source, name)
                if os.path.isfile(source_file):
                    destination = os.path.join(kit_destination, name)
                    operations.append((destination, source_file, None))
                    copied.append(destination)
            copied_kits.add(kit_id)

        metadata = source.get("metadata", {})
        if metadata.get("show_in_steam") and not exported["cover"]:
            raise MakerLiteExportError(
                f"{source['title']}: Show in Steam requires readable cover art"
            )
        cover_device_path = ""
        if metadata.get("show_in_steam"):
            cover_destination = os.path.join(
                project_device_dir, "cover.144x108x24.bmp"
            )
            operations.append((cover_destination, exported["cover"], None))
            copied.append(cover_destination)
            cover_device_path = (
                f"/{DEVICE_ROOT}/projects/{project_id}/"
                "cover.144x108x24.bmp"
            )
        fields = [
            source["title"],
            PLUGIN_PATH,
            cover_device_path,
            "0",
            f"/{DEVICE_ROOT}/saves/{project_id}.sav",
            "0",
            str(metadata.get("genre", "")),
            str(metadata.get("author", "")),
            str(metadata.get("author", "")),
            str(metadata.get("description", ""))
            .replace("\t", " ")
            .replace("\n", " "),
            f"/{DEVICE_ROOT}/projects/{project_id}/game.mlp",
        ]
        browser_rows.append("\t".join(fields))
        if metadata.get("show_in_steam"):
            rows.append("\t".join(fields))

    if plugin_source:
        if not os.path.isfile(plugin_source):
            raise MakerLiteExportError("Maker Lite plugin build is missing")
        destination = os.path.join(mount_root, PLUGIN_PATH.lstrip("/"))
        operations.append((destination, plugin_source, None))
        copied.append(destination)

    manifest = os.path.join(mount_root, MANIFEST_PATH)
    browser_manifest = os.path.join(mount_root, BROWSER_MANIFEST_PATH)
    manifest_data = "".join(
        row + "\n" for row in sorted(rows, key=str.casefold)
    ).encode("utf-8")
    browser_manifest_data = "".join(
        row + "\n" for row in sorted(browser_rows, key=str.casefold)
    ).encode("utf-8")

    # Nothing under the mounted device is replaced until every project,
    # settings blob, private kit, cover, plugin, and manifest has passed
    # validation and has been staged successfully.
    staged = []
    try:
        for destination, source, data in operations:
            staged.append(
                (_stage_file(destination, source=source, data=data), destination)
            )
        # Commit both authoritative indexes after their project content. The
        # all-project browser index is separate from the cover-gated Steam
        # catalog so a synced project can always launch from Classic Games.
        staged.append(
            (
                _stage_file(browser_manifest, data=browser_manifest_data),
                browser_manifest,
            )
        )
        staged.append((_stage_file(manifest, data=manifest_data), manifest))
        _commit_staged(staged)
    except Exception:
        for temporary, _destination in staged:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass
        raise
    copied.append(manifest)
    copied.append(browser_manifest)
    return {
        "copied": copied,
        "manifest": manifest,
        "browser_manifest": browser_manifest,
        "rows": len(rows),
        "browser_rows": len(browser_rows),
    }
