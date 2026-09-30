"""Desktop Mode device status, verified install, and host parity preview.

All Apple-owned imagery stays in the ignored private asset area or on the
user's device.  Host preview copies the selected pack into an isolated
simulator root and never reads UI assets from the device while the simulator
is painting.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path

from services.file_safety import atomic_write_json
from services.path_safety import resolve_under_root, validate_device_root
from services.snow_leopard_assets import (
    DEFAULT_PACK_DIR,
    DEVICE_PACK_RELATIVE,
    SIMULATOR_PACK_RELATIVE,
    SnowLeopardAssetError,
    build_pack,
    discover_assets,
    install_pack,
    validate_pack,
)


DESKTOP_PLUGIN_RELATIVE = ".rockbox/rocks/apps/desktop_mode.rock"
SITEKICK_PLUGIN_RELATIVE = ".rockbox/rocks/apps/sitekick.rock"
NETFLIX_PLUGIN_RELATIVE = ".rockbox/rocks/apps/netflix_desktop.rock"
SITEKICK_DATA_RELATIVE = ".rockbox/sitekick"
NETFLIX_DATA_RELATIVE = ".rockbox/ipodjs/netflix"
DESKTOP_CONFIG_RELATIVE = ".rockbox/rocks/apps/desktop_mode.cfg"
# Simulator plugins are built with the same PLUGIN_APPS_DATA_DIR as hardware:
# `/.rockbox/rocks/apps`.  Staging under `rocks.data` makes the plugin return
# immediately because neither its manifest nor its first boot asset exists at
# the compiled path.
SIMULATOR_CONFIG_RELATIVE = DESKTOP_CONFIG_RELATIVE
PARITY_FORMAT = 1

# Fallback panel size when a preview carries no panel resolution of its own
# (e.g. a caller driving launch_parity_preview() directly against an older
# 320x240-only simulator target).
LCD_WIDTH = 320
LCD_HEIGHT = 240
# The parity session should fill the host screen rather than sit in a
# postage stamp, but the scale stays a whole number: the simulator switches to
# nearest-neighbour above 1x, so an integer factor keeps every Snow Leopard
# pixel square and inspectable.  A fractional factor would resample the real
# chrome and undo the point of importing it at 1:1.
MIN_DISPLAY_ZOOM = 1
MAX_DISPLAY_ZOOM = 8
# leave room for the window frame and any panel or dock on the host desktop
HOST_SCREEN_MARGIN = 0.92


def host_display_zoom(screen_size=None, panel_size=None):
    """Largest whole-number scale of the panel that fits `screen_size`.

    `screen_size` is an (width, height) pair in host pixels, normally the
    available geometry of the screen Rockpod is on.  Without one, the host
    display is measured directly, and failing that the scale falls back to 2x,
    which fits every display Rockpod runs on.  `panel_size` is the (width,
    height) of the Desktop Mode panel being previewed; it defaults to the
    320x240 iPod panel for callers that don't know any better.
    """
    panel_width, panel_height = panel_size or (LCD_WIDTH, LCD_HEIGHT)
    if screen_size is None:
        screen_size = _detect_screen_size()
    if not screen_size:
        return 2
    width, height = screen_size
    if width <= 0 or height <= 0:
        return 2
    zoom = min(
        int(width * HOST_SCREEN_MARGIN) // panel_width,
        int(height * HOST_SCREEN_MARGIN) // panel_height,
    )
    return max(MIN_DISPLAY_ZOOM, min(MAX_DISPLAY_ZOOM, zoom))


def _detect_screen_size():
    """Measure the host screen without importing a UI toolkit into a service."""
    try:
        output = subprocess.run(
            ["xrandr", "--current"],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        ).stdout
    except (OSError, TypeError, ValueError, subprocess.SubprocessError):
        return None
    match = re.search(r"\bcurrent\s+(\d+)\s*x\s*(\d+)", output)
    if not match:
        match = re.search(r"\b(\d+)x(\d+)\+\d+\+\d+", output)
    if not match:
        return None
    return int(match.group(1)), int(match.group(2))


class DesktopModeError(RuntimeError):
    """Raised when Desktop Mode cannot be installed or previewed safely."""


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def _parse_resolution(value) -> tuple[int, int]:
    """Parse a "WIDTHxHEIGHT" screen_resolution string, defaulting to 320x240."""
    match = re.match(r"^\s*(\d+)x(\d+)\s*$", str(value or ""))
    if not match:
        return LCD_WIDTH, LCD_HEIGHT
    return int(match.group(1)), int(match.group(2))


def _safe_key(value: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._-]+", "-", str(value or "")).strip(".-")
    if cleaned:
        return cleaned[:80]
    return hashlib.sha256(str(value or "desktop-mode").encode("utf-8")).hexdigest()[:16]


class DesktopModeService:
    """Coordinate the private asset pack with device and simulator targets."""

    def __init__(self, repo_root: str | os.PathLike[str]):
        self.repo_root = Path(repo_root).expanduser().resolve()
        self.private_root = self.repo_root / "rockpod" / ".desktop_mode"
        self.local_pack_dir = Path(DEFAULT_PACK_DIR).resolve()
        self._preview_processes = []
        self._pack_status_cache = {}

    def _reap_previews(self) -> None:
        self._preview_processes = [
            process
            for process in self._preview_processes
            if process.poll() is None
        ]

    @staticmethod
    def pack_status(pack_dir: str | os.PathLike[str]) -> dict:
        result = validate_pack(pack_dir)
        manifest = result.get("manifest") or {}
        return {
            "path": result.get("root") or str(Path(pack_dir).resolve()),
            "valid": bool(result.get("valid")),
            "complete": bool(result.get("complete")),
            "product_version": str(manifest.get("product_version") or ""),
            "pack_id": str(manifest.get("pack_id") or ""),
            "asset_count": len(result.get("checked") or []),
            "errors": list(result.get("errors") or []),
            "missing": list(result.get("missing") or []),
        }

    @staticmethod
    def _pack_signature(pack_dir: str | os.PathLike[str]) -> tuple:
        """Cheaply identify unchanged packs without rereading every asset."""
        root = Path(pack_dir).expanduser().resolve()
        try:
            if not root.is_dir():
                return ("missing",)
            files = []
            for path in sorted(root.rglob("*")):
                if not path.is_file():
                    continue
                stat = path.stat()
                files.append(
                    (
                        path.relative_to(root).as_posix(),
                        stat.st_size,
                        stat.st_mtime_ns,
                    )
                )
            return tuple(files)
        except OSError:
            return ("unreadable",)

    def _cached_pack_status(self, pack_dir: str | os.PathLike[str]) -> dict:
        root = Path(pack_dir).expanduser().resolve()
        key = str(root)
        signature = self._pack_signature(root)
        cached = self._pack_status_cache.get(key)
        if cached and cached[0] == signature:
            return dict(cached[1])
        result = self.pack_status(root)
        self._pack_status_cache[key] = (signature, result)
        return dict(result)

    def _invalidate_pack_status(self, pack_dir: str | os.PathLike[str]) -> None:
        key = str(Path(pack_dir).expanduser().resolve())
        self._pack_status_cache.pop(key, None)

    def status(self, device=None) -> dict:
        self._reap_previews()
        local = self._cached_pack_status(self.local_pack_dir)
        connected = bool(device and getattr(device, "mount_path", ""))
        device_status = {
            "path": "",
            "valid": False,
            "complete": False,
            "product_version": "",
            "pack_id": "",
            "asset_count": 0,
            "errors": ["No iPod connected"],
            "missing": [],
        }
        plugin_present = False
        plugin_path = ""
        if connected:
            root = Path(getattr(device, "mount_path")).expanduser().resolve()
            device_status = self._cached_pack_status(root / DEVICE_PACK_RELATIVE)
            plugin_path = str(root / DESKTOP_PLUGIN_RELATIVE)
            plugin_present = Path(plugin_path).is_file()
        return {
            "local": local,
            "device": device_status,
            "device_connected": connected,
            "device_name": str(getattr(device, "name", "") or "") if connected else "",
            "device_target": str(getattr(device, "rockbox_target", "") or "") if connected else "",
            "device_plugin_present": plugin_present,
            "device_plugin_path": plugin_path,
            "host_preview_count": len(self._preview_processes),
        }

    def build_private_pack(self, source_roots) -> dict:
        try:
            return build_pack(source_roots, self.local_pack_dir)
        except SnowLeopardAssetError as exc:
            raise DesktopModeError(str(exc)) from exc
        finally:
            self._invalidate_pack_status(self.local_pack_dir)

    @staticmethod
    def inspect_private_sources(source_roots) -> dict:
        try:
            return discover_assets(source_roots)
        except SnowLeopardAssetError as exc:
            raise DesktopModeError(str(exc)) from exc

    def install_private_pack(self, device) -> dict:
        if not device or not getattr(device, "mount_path", ""):
            raise DesktopModeError("Connect a Rockbox iPod before installing Desktop Mode")
        device_pack = (
            Path(device.mount_path).expanduser().resolve()
            / DEVICE_PACK_RELATIVE
        )
        try:
            result = install_pack(self.local_pack_dir, device.mount_path)
        except SnowLeopardAssetError as exc:
            raise DesktopModeError(str(exc)) from exc
        finally:
            self._invalidate_pack_status(device_pack)
        result["plugin_present"] = Path(
            resolve_under_root(validate_device_root(device.mount_path), DESKTOP_PLUGIN_RELATIVE)
        ).is_file()
        return result

    @staticmethod
    def choose_simulator_target(targets, device=None) -> dict | None:
        all_targets = list(targets or [])
        # The 1080p panel target exists specifically so this feature can fill
        # the host screen with full-size Snow Leopard chrome instead of
        # zooming a 320x240 postage stamp, so prefer it whenever it's built.
        hires = [
            target
            for target in all_targets
            if str(target.get("screen_resolution") or "") == "1920x1080"
        ]
        if hires:
            return hires[0]
        candidates = [
            target
            for target in all_targets
            if str(target.get("screen_resolution") or "") == "320x240"
        ]
        if not candidates:
            return None
        rockbox_target = str(getattr(device, "rockbox_target", "") or "").lower()
        preferred = []
        if rockbox_target == "ipod6g":
            preferred = ["build-sim-ipod6g"]
        elif rockbox_target in {"ipodvideo", "ipodvideo64mb"}:
            preferred = ["build-sim-video-5g"]
        preferred.extend(["build-sim-ipod6g", "build-sim-video-5g"])
        for target_id in preferred:
            for target in candidates:
                if target.get("id") == target_id:
                    return target
        return candidates[0]

    def prepare_parity_preview(
        self,
        target: dict,
        *,
        device=None,
        use_device_pack: bool,
    ) -> dict:
        if not target:
            raise DesktopModeError("No compatible Rockbox simulator is available")
        panel_width, panel_height = _parse_resolution(target.get("screen_resolution"))
        binary = Path(str(target.get("binary_path") or "")).resolve()
        base_simdisk = Path(str(target.get("simdisk_path") or "")).resolve()
        build_dir = Path(str(target.get("build_dir") or "")).resolve()
        if not binary.is_file() or not base_simdisk.is_dir():
            raise DesktopModeError("The selected Rockbox simulator target is incomplete")

        if use_device_pack:
            if not device or not getattr(device, "mount_path", ""):
                raise DesktopModeError("Connect the iPod whose Desktop Mode you want to display")
            source_root = Path(validate_device_root(device.mount_path))
            pack_source = Path(resolve_under_root(source_root, DEVICE_PACK_RELATIVE))
            config_source = Path(resolve_under_root(source_root, DESKTOP_CONFIG_RELATIVE))
            identity = (
                getattr(device, "stable_device_key", "")
                or getattr(device, "mount_path", "")
                or "connected-ipod"
            )
            source_kind = "connected-device"
        else:
            source_root = None
            pack_source = self.local_pack_dir
            config_source = None
            identity = "local-private-pack"
            source_kind = "local-private-pack"

        validation = validate_pack(pack_source)
        if not validation["valid"]:
            raise DesktopModeError(
                "Desktop Mode preview requires a complete verified Snow Leopard pack: "
                + "; ".join(validation["errors"])
            )

        target_key = _safe_key(str(target.get("id") or "simulator"))
        preview_root = self.private_root / "parity" / _safe_key(identity) / target_key
        preview_root.parent.mkdir(parents=True, exist_ok=True)
        excluded_simdisk_trees = {
            Path(SIMULATOR_PACK_RELATIVE),
            Path(".rockbox/rocks/apps/desktop_mode_snow_leopard"),
            Path(".rockbox/rocks.data/desktop_mode_xp"),
            Path(".rockbox/rocks/apps/desktop_mode_xp"),
        }

        def ignore_replaced_trees(directory, names):
            try:
                relative = Path(directory).resolve().relative_to(base_simdisk)
            except ValueError:
                return []
            ignored = [
                name
                for name in names
                if relative / name in excluded_simdisk_trees
            ]
            if source_root is not None and relative == Path("."):
                ignored.extend(name for name in names if name != ".rockbox")
            return sorted(set(ignored))

        with tempfile.TemporaryDirectory(
            prefix="desktop-mode-parity-",
            dir=preview_root.parent,
        ) as temporary:
            stage_root = Path(temporary) / "preview"
            stage_simdisk = stage_root / "simdisk"
            shutil.copytree(
                base_simdisk,
                stage_simdisk,
                ignore=ignore_replaced_trees,
            )

            linked_device_entries = []
            copied_tagcache = []
            removed_playback_state = []
            videolist_copied = False
            sitekick_copied = False
            netflix_assets_copied = False
            if source_root is not None:
                # The host preview is a live view of the connected iPod, not
                # the simulator fixture library. Keep the simulator's private
                # Rockbox runtime, then expose every device-root entry through
                # a symlink so Finder and players open the real mounted files.
                entries = sorted(
                    source_root.iterdir(),
                    key=lambda item: item.name.casefold(),
                )
                for source in entries:
                    if source.name == ".rockbox":
                        continue
                    destination = stage_simdisk / source.name
                    destination.symlink_to(
                        source,
                        target_is_directory=source.is_dir(),
                    )
                    linked_device_entries.append(source.name)

                stage_rockbox = stage_simdisk / ".rockbox"
                device_rockbox = source_root / ".rockbox"

                # A parity session must not inherit the build simulator's last
                # playlist or resume point.  Besides being unrelated to this
                # iPod, that stale state can spend seconds probing missing
                # paths before Desktop Mode receives its first interaction.
                for pattern in (".playlist_control*", ".resume.cfg*"):
                    for stale in stage_rockbox.glob(pattern):
                        if stale.is_file() or stale.is_symlink():
                            stale.unlink()
                            removed_playback_state.append(stale.name)

                # Never leave the base simulator's hardcoded video catalog in
                # a connected-device session. Copy the device catalog and its
                # artwork if present; otherwise iTunes Videos is empty while
                # Finder still exposes every on-device video directory.
                videolist_destination = stage_rockbox / "videolist"
                if videolist_destination.is_symlink() or videolist_destination.is_file():
                    videolist_destination.unlink()
                elif videolist_destination.is_dir():
                    shutil.rmtree(videolist_destination)
                videolist_source = device_rockbox / "videolist"
                if videolist_source.is_dir():
                    shutil.copytree(videolist_source, videolist_destination)
                    videolist_copied = True

                # Sitekick is a mounted-iPod app too. Use an isolated copy of
                # its catalogue and save state so the desktop view matches the
                # device without letting a preview session mutate the mount.
                sitekick_source = device_rockbox / "sitekick"
                sitekick_destination = stage_rockbox / "sitekick"
                if sitekick_source.is_dir():
                    if sitekick_destination.is_symlink() or \
                            sitekick_destination.is_file():
                        sitekick_destination.unlink()
                    elif sitekick_destination.is_dir():
                        shutil.rmtree(sitekick_destination)
                    shutil.copytree(sitekick_source, sitekick_destination)
                    sitekick_copied = True

                netflix_source = device_rockbox / "ipodjs" / "netflix"
                netflix_destination = stage_rockbox / "ipodjs" / "netflix"
                if netflix_source.is_dir():
                    if netflix_destination.is_symlink() or \
                            netflix_destination.is_file():
                        netflix_destination.unlink()
                    elif netflix_destination.is_dir():
                        shutil.rmtree(netflix_destination)
                    netflix_destination.parent.mkdir(
                        parents=True, exist_ok=True
                    )
                    shutil.copytree(netflix_source, netflix_destination)
                    netflix_assets_copied = True

                # iTunes Songs/Albums/Artists uses tagcache. Work from a copy:
                # the desktop simulator must never write into the mounted
                # iPod's live database files.
                for pattern in ("database*.tcd", "tagcache*.tcd"):
                    for stale in stage_rockbox.glob(pattern):
                        if stale.is_file() or stale.is_symlink():
                            stale.unlink()
                    if device_rockbox.is_dir():
                        for source in sorted(device_rockbox.glob(pattern)):
                            if not source.is_file():
                                continue
                            shutil.copy2(source, stage_rockbox / source.name)
                            copied_tagcache.append(source.name)

            pack_destination = stage_simdisk / DEVICE_PACK_RELATIVE
            pack_destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(pack_source, pack_destination)
            staged_validation = validate_pack(pack_destination)
            if not staged_validation["valid"]:
                raise DesktopModeError(
                    "The isolated simulator copy failed pack verification: "
                    + "; ".join(staged_validation["errors"])
                )

            plugins_to_stage = [
                ("desktop_mode", DESKTOP_PLUGIN_RELATIVE),
                ("sitekick", SITEKICK_PLUGIN_RELATIVE),
            ]
            if panel_width >= 1920:
                plugins_to_stage.append(
                    ("netflix_desktop", NETFLIX_PLUGIN_RELATIVE)
                )
            for plugin_name, plugin_relative in plugins_to_stage:
                plugin_candidates = (
                    build_dir / f"apps/plugins/{plugin_name}.rock",
                    base_simdisk / plugin_relative,
                )
                plugin_source = next(
                    (path for path in plugin_candidates if path.is_file()),
                    None,
                )
                if plugin_source is None:
                    raise DesktopModeError(
                        f"{plugin_name}.rock is missing; build the selected "
                        "simulator's rocks target"
                    )
                plugin_destination = stage_simdisk / plugin_relative
                plugin_destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(plugin_source, plugin_destination)

            config_destination = stage_simdisk / SIMULATOR_CONFIG_RELATIVE
            if config_source and config_source.is_file():
                config_destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(config_source, config_destination)
            elif config_destination.exists():
                config_destination.unlink()

            parity_manifest = {
                "format": PARITY_FORMAT,
                "created_at": _utc_now(),
                "source_kind": source_kind,
                "source_pack": str(pack_source),
                "pack_id": staged_validation["manifest"]["pack_id"],
                "product_version": staged_validation["manifest"]["product_version"],
                "simulator_target": str(target.get("id") or ""),
                "config_copied": bool(config_source and config_source.is_file()),
                "media_source_root": str(source_root) if source_root else "",
                "linked_device_entries": linked_device_entries,
                "tagcache_files_copied": sorted(set(copied_tagcache)),
                "playback_state_removed": sorted(
                    set(removed_playback_state)
                ),
                "videolist_copied": videolist_copied,
                "sitekick_copied": sitekick_copied,
                "netflix_assets_copied": netflix_assets_copied,
                "isolated": True,
                "panel_width": panel_width,
                "panel_height": panel_height,
            }
            atomic_write_json(stage_root / "parity-manifest.json", parity_manifest)
            if preview_root.exists():
                shutil.rmtree(preview_root)
            os.replace(stage_root, preview_root)

        return {
            "preview_root": str(preview_root),
            "simdisk_path": str(preview_root / "simdisk"),
            "binary_path": str(binary),
            "build_dir": str(build_dir),
            "target_id": str(target.get("id") or ""),
            "manifest": parity_manifest,
        }

    def launch_parity_preview(
        self, preview: dict, *, screen_size=None, fullscreen: bool = True
    ) -> dict:
        binary = Path(preview["binary_path"])
        simdisk = Path(preview["simdisk_path"])
        build_dir = Path(preview["build_dir"])
        if not binary.is_file() or not simdisk.is_dir():
            raise DesktopModeError("The prepared Desktop Mode preview no longer exists")

        log_dir = Path(preview["preview_root"]) / "logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        log_path = log_dir / f"desktop-mode-{datetime.now().strftime('%Y%m%d-%H%M%S')}.log"
        env = os.environ.copy()
        env["RBROOT"] = preview["preview_root"]
        env["ROCKBOX_SIM_PLUGIN"] = "/.rockbox/rocks/apps/desktop_mode.rock"
        env["ROCKBOX_SIM_PLUGIN_PARAM"] = "simulator-desktop"
        env["ROCKBOX_SIM_PLUGIN_EXIT"] = "1"
        # The person driving the parity session is sitting at a computer, so
        # the session fills their display and their own mouse moves the Snow
        # Leopard pointer.  The simulator publishes the host pointer in panel
        # coordinates through this record; Desktop Mode reads it directly.
        pointer_path = simdisk / ".rockbox" / "host-pointer"
        pointer_path.parent.mkdir(parents=True, exist_ok=True)
        env["ROCKPOD_SIM_HOST_POINTER"] = str(pointer_path)
        manifest = preview.get("manifest") or {}
        panel_width = int(manifest.get("panel_width") or LCD_WIDTH)
        panel_height = int(manifest.get("panel_height") or LCD_HEIGHT)
        zoom = host_display_zoom(screen_size, panel_size=(panel_width, panel_height))
        command = [str(binary), "--nobackground"]
        if fullscreen:
            command.append("--fullscreen")
        else:
            command.extend(["--zoom", str(zoom)])
        command.extend(["--root", str(simdisk)])
        log_handle = log_path.open("wb")
        try:
            process = subprocess.Popen(
                command,
                cwd=build_dir,
                env=env,
                stdout=log_handle,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
        finally:
            log_handle.close()
        self._reap_previews()
        self._preview_processes.append(process)
        return {
            "pid": process.pid,
            "command": command,
            "preview_root": preview["preview_root"],
            "log_path": str(log_path),
            "source_kind": preview["manifest"]["source_kind"],
            "fullscreen": bool(fullscreen),
            "display_zoom": zoom,
            "display_size": (panel_width * zoom, panel_height * zoom),
            "host_pointer_path": str(pointer_path),
        }

    def stop_host_previews(self) -> int:
        self._reap_previews()
        stopped = 0
        for process in list(self._preview_processes):
            try:
                process.terminate()
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
            except OSError:
                pass
            stopped += 1
        self._preview_processes = []
        return stopped

    def shutdown(self) -> None:
        self.stop_host_previews()

    def display_on_host(
        self,
        targets,
        *,
        device=None,
        use_device_pack: bool,
        screen_size=None,
        fullscreen: bool = True,
    ) -> dict:
        target = self.choose_simulator_target(targets, device if use_device_pack else None)
        preview = self.prepare_parity_preview(
            target,
            device=device,
            use_device_pack=use_device_pack,
        )
        return self.launch_parity_preview(
            preview, screen_size=screen_size, fullscreen=fullscreen
        )
