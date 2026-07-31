"""Local PDF library and device sync for the Rockbox Magazines plugin."""

from __future__ import annotations

import os
from pathlib import Path
import json
import hashlib
import re
import shutil
import sys
import tempfile


MAGAZINE_TARGET_DIR = "Magazines"
DEFAULT_CATEGORY = "Uncategorized"
METADATA_NAME = "library.json"
SAFE_ID_RE = re.compile(r"[^a-z0-9]+")


class MagazineSyncError(RuntimeError):
    """A user-facing magazine import or sync failure."""


def _issue_id(value):
    issue_id = SAFE_ID_RE.sub("-", str(value or "").strip().lower()).strip("-")
    return issue_id[:96] or "magazine"


def _manifest(path):
    values = {}
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError):
        return values
    for line in lines:
        if not line or line.lstrip().startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def _sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class RockboxMagazineService:
    """Import PDFs, prepare reader packages, and sync selected issues."""

    def __init__(self, config, repo_root):
        self._config = config
        self._repo_root = os.path.abspath(repo_root)

    @property
    def library_root(self):
        configured = self._config.get("magazines_library_path", "")
        return os.path.abspath(
            configured
            or os.path.join(
                self._config.get("cache_dir"), "..", "magazines"
            )
        )

    @property
    def pdf_root(self):
        return os.path.join(self.library_root, "pdfs")

    @property
    def prepared_root(self):
        return os.path.join(self.library_root, "prepared")

    @property
    def metadata_path(self):
        return os.path.join(self.library_root, METADATA_NAME)

    def ensure_library(self):
        os.makedirs(self.pdf_root, exist_ok=True)
        os.makedirs(self.prepared_root, exist_ok=True)

    def import_pdf(self, source_path):
        source = os.path.abspath(source_path)
        if not os.path.isfile(source) or Path(source).suffix.lower() != ".pdf":
            raise MagazineSyncError("Choose an existing PDF file.")
        with open(source, "rb") as handle:
            if handle.read(5) != b"%PDF-":
                raise MagazineSyncError(f"{os.path.basename(source)} is not a PDF.")

        self.ensure_library()
        source_size = os.path.getsize(source)
        source_digest = None
        for existing in sorted(Path(self.pdf_root).glob("*.pdf")):
            try:
                if existing.stat().st_size != source_size:
                    continue
            except OSError:
                continue
            if source_digest is None:
                source_digest = _sha256_file(source)
            if _sha256_file(existing) == source_digest:
                return existing.stem
        base_id = _issue_id(Path(source).stem)
        issue_id = base_id
        suffix = 2
        while os.path.exists(os.path.join(self.pdf_root, issue_id + ".pdf")):
            issue_id = f"{base_id}-{suffix}"
            suffix += 1
        destination = os.path.join(self.pdf_root, issue_id + ".pdf")
        fd, temporary = tempfile.mkstemp(
            prefix=f".{issue_id}.", suffix=".pdf", dir=self.pdf_root
        )
        os.close(fd)
        try:
            shutil.copy2(source, temporary)
            os.replace(temporary, destination)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        return issue_id

    @staticmethod
    def _clean_category(value):
        raw = str(value or "")
        if any(char in raw for char in "\r\n="):
            raise MagazineSyncError(
                "Category names must be 1–48 characters without line breaks."
            )
        category = " ".join(raw.split())
        if not category:
            category = DEFAULT_CATEGORY
        if len(category) > 48:
            raise MagazineSyncError(
                "Category names must be 1–48 characters without line breaks."
            )
        return category

    def _load_library_metadata(self):
        default = {"schema": 1, "categories": [DEFAULT_CATEGORY], "issues": {}}
        try:
            with open(self.metadata_path, "r", encoding="utf-8") as handle:
                loaded = json.load(handle)
        except (OSError, UnicodeError, json.JSONDecodeError):
            return default
        if not isinstance(loaded, dict):
            return default
        categories = []
        for value in loaded.get("categories", []):
            try:
                category = self._clean_category(value)
            except MagazineSyncError:
                continue
            if category not in categories:
                categories.append(category)
        if DEFAULT_CATEGORY not in categories:
            categories.insert(0, DEFAULT_CATEGORY)
        issues = loaded.get("issues", {})
        if not isinstance(issues, dict):
            issues = {}
        prepared_root = Path(self.prepared_root)
        if prepared_root.is_dir():
            for path in prepared_root.iterdir():
                if not path.is_dir():
                    continue
                manifest = _manifest(path / "issue.mgi")
                try:
                    category = self._clean_category(
                        manifest.get("category", DEFAULT_CATEGORY)
                    )
                except MagazineSyncError:
                    category = DEFAULT_CATEGORY
                if category not in categories:
                    categories.append(category)
                values = issues.setdefault(path.name, {})
                values.setdefault("category", category)
                values.setdefault(
                    "locked", str(manifest.get("locked", "0")) == "1"
                )
        return {"schema": 1, "categories": categories, "issues": issues}

    def _save_library_metadata(self, metadata):
        self.ensure_library()
        fd, temporary = tempfile.mkstemp(
            prefix=".library.", suffix=".json", dir=self.library_root
        )
        try:
            with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as output:
                json.dump(metadata, output, indent=2, sort_keys=True)
                output.write("\n")
            os.replace(temporary, self.metadata_path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    def categories(self):
        return list(self._load_library_metadata()["categories"])

    def set_issue_category(self, issue_ids, category):
        category = self._clean_category(category)
        metadata = self._load_library_metadata()
        if category not in metadata["categories"]:
            metadata["categories"].append(category)
        for issue_id in issue_ids:
            issue_id = _issue_id(issue_id)
            values = metadata["issues"].setdefault(issue_id, {})
            values["category"] = category
            self._update_prepared_manifest(issue_id, category=category)
        self._save_library_metadata(metadata)

    def rename_category(self, old_name, new_name):
        old_name = self._clean_category(old_name)
        new_name = self._clean_category(new_name)
        if old_name == DEFAULT_CATEGORY:
            raise MagazineSyncError("Uncategorized cannot be renamed.")
        metadata = self._load_library_metadata()
        if old_name not in metadata["categories"]:
            raise MagazineSyncError(f"Category does not exist: {old_name}")
        metadata["categories"] = [
            new_name if value == old_name else value
            for value in metadata["categories"]
        ]
        metadata["categories"] = list(dict.fromkeys(metadata["categories"]))
        for issue_id, values in metadata["issues"].items():
            if values.get("category") == old_name:
                values["category"] = new_name
                self._update_prepared_manifest(issue_id, category=new_name)
        self._save_library_metadata(metadata)

    def delete_category(self, category):
        category = self._clean_category(category)
        if category == DEFAULT_CATEGORY:
            raise MagazineSyncError("Uncategorized cannot be deleted.")
        metadata = self._load_library_metadata()
        metadata["categories"] = [
            value for value in metadata["categories"] if value != category
        ]
        for issue_id, values in metadata["issues"].items():
            if values.get("category") == category:
                values["category"] = DEFAULT_CATEGORY
                self._update_prepared_manifest(
                    issue_id, category=DEFAULT_CATEGORY
                )
        self._save_library_metadata(metadata)

    def set_issues_locked(self, issue_ids, locked):
        metadata = self._load_library_metadata()
        for issue_id in issue_ids:
            issue_id = _issue_id(issue_id)
            values = metadata["issues"].setdefault(issue_id, {})
            values["locked"] = bool(locked)
            self._update_prepared_manifest(issue_id, locked=bool(locked))
        self._save_library_metadata(metadata)

    def _update_prepared_manifest(self, issue_id, **updates):
        path = Path(self.prepared_root) / issue_id / "issue.mgi"
        if not path.is_file():
            return
        values = _manifest(path)
        values.update(
            {
                key: ("1" if value else "0") if key == "locked" else str(value)
                for key, value in updates.items()
            }
        )
        temporary = path.with_name("issue.mgi.tmp")
        temporary.write_text(
            "".join(f"{key}={value}\n" for key, value in values.items()),
            encoding="utf-8",
        )
        os.replace(temporary, path)

    def preparation_command(self, issue_id, profile="standard"):
        issue_id = _issue_id(issue_id)
        if profile not in {"standard", "fine-text"}:
            raise MagazineSyncError(
                f"Unknown magazine preparation profile: {profile}"
            )
        pdf = os.path.join(self.pdf_root, issue_id + ".pdf")
        if not os.path.isfile(pdf):
            raise MagazineSyncError(f"Imported PDF is missing: {pdf}")
        tool = os.path.join(self._repo_root, "tools", "magazine_prepare.py")
        if not os.path.isfile(tool):
            raise MagazineSyncError("Magazine preparation tool is missing.")
        library = self._load_library_metadata()
        issue_values = library["issues"].get(issue_id, {})
        command = [
            sys.executable,
            tool,
            "--pdf",
            pdf,
            "--output",
            os.path.join(self.prepared_root, issue_id),
            "--issue-id",
            issue_id,
            "--title",
            Path(pdf).stem.replace("-", " ").title(),
            "--profile",
            profile,
        ]
        command.extend(
            ["--category", issue_values.get("category", DEFAULT_CATEGORY)]
        )
        if bool(issue_values.get("locked", False)):
            command.append("--locked")
        command.append("--force")
        return command

    def target_root(self, profile, target_mode="device", simulator_target=None):
        mount = str(profile.get("device_mount_path") or "")
        if target_mode == "simulator":
            mount = str(
                profile.get("simulator_simdisk_path")
                or (simulator_target or {}).get("simdisk_path")
                or ""
            )
        if not mount:
            return ""
        return os.path.join(os.path.abspath(mount), MAGAZINE_TARGET_DIR)

    def list_issues(self, profile=None, target_mode="device", simulator_target=None):
        self.ensure_library()
        library = self._load_library_metadata()
        target = (
            self.target_root(profile, target_mode, simulator_target)
            if profile
            else ""
        )
        issue_ids = {
            path.stem
            for path in Path(self.pdf_root).glob("*.pdf")
            if path.is_file()
        }
        issue_ids.update(
            path.name
            for path in Path(self.prepared_root).iterdir()
            if path.is_dir()
        )
        issues = []
        for issue_id in sorted(issue_ids, key=str.casefold):
            pdf = os.path.join(self.pdf_root, issue_id + ".pdf")
            prepared = os.path.join(self.prepared_root, issue_id)
            metadata = _manifest(Path(prepared) / "issue.mgi")
            saved = library["issues"].get(issue_id, {})
            category = self._clean_category(
                saved.get("category", metadata.get("category", DEFAULT_CATEGORY))
            )
            locked = bool(
                saved.get(
                    "locked", str(metadata.get("locked", "0")) == "1"
                )
            )
            valid = self._valid_prepared_issue(prepared, metadata)
            issues.append(
                {
                    "id": issue_id,
                    "title": metadata.get(
                        "title", issue_id.replace("-", " ").title()
                    ),
                    "creator": metadata.get("creator", ""),
                    "page_count": int(metadata.get("page_count", "0") or 0),
                    "prepare_profile": metadata.get(
                        "prepare_profile", "standard" if valid else ""
                    ),
                    "category": category,
                    "locked": locked,
                    "pdf_path": pdf if os.path.isfile(pdf) else "",
                    "prepared_path": prepared if valid else "",
                    "prepared": valid,
                    "on_target": bool(
                        target
                        and self._valid_prepared_issue(
                            os.path.join(target, issue_id)
                        )
                    ),
                }
            )
        return issues

    def sync_issues(
        self, profile, issues, target_mode="device", simulator_target=None,
        locked_pin="",
    ):
        target = self.target_root(profile, target_mode, simulator_target)
        if not target:
            raise MagazineSyncError("No device or simulator target is configured.")
        mount = os.path.dirname(target)
        if not os.path.isdir(mount):
            raise MagazineSyncError(f"Target mount is unavailable: {mount}")
        locked_pin = str(locked_pin or "").strip()
        if any(bool(issue.get("locked")) for issue in issues):
            if len(locked_pin) != 4 or not locked_pin.isdigit():
                raise MagazineSyncError(
                    "A 4-digit Locked Videos/Magazines PIN is required."
                )
        os.makedirs(target, exist_ok=True)
        copied = 0
        for issue in issues:
            issue_id = _issue_id(issue.get("id"))
            source = os.path.join(self.prepared_root, issue_id)
            if not self._valid_prepared_issue(source):
                raise MagazineSyncError(
                    f"{issue.get('title') or issue_id} is not prepared."
                )
            self._replace_directory(source, os.path.join(target, issue_id))
            copied += 1
        if any(bool(issue.get("locked")) for issue in issues):
            pin_dir = os.path.join(mount, ".rockbox", "videolist")
            os.makedirs(pin_dir, exist_ok=True)
            temporary_pin = os.path.join(pin_dir, "locked.pin.tmp")
            with open(
                temporary_pin, "w", encoding="ascii", newline="\n"
            ) as output:
                output.write(locked_pin + "\n")
            os.replace(temporary_pin, os.path.join(pin_dir, "locked.pin"))
        self._write_catalog(target)
        return copied

    def remove_issues(
        self, profile, issues, target_mode="device", simulator_target=None
    ):
        target = self.target_root(profile, target_mode, simulator_target)
        if not target or not os.path.isdir(target):
            return 0
        removed = 0
        for issue in issues:
            destination = os.path.join(target, _issue_id(issue.get("id")))
            if os.path.isdir(destination):
                shutil.rmtree(destination)
                removed += 1
        self._write_catalog(target)
        return removed

    @staticmethod
    def _valid_prepared_issue(path, metadata=None):
        root = Path(path)
        metadata = metadata if metadata is not None else _manifest(root / "issue.mgi")
        try:
            count = int(metadata.get("page_count", "0"))
        except (TypeError, ValueError):
            return False
        return bool(
            count > 0
            and (root / "cover.jpg").is_file()
            and (root / "pages" / "0001.jpg").is_file()
            and (root / "pages" / f"{count:04d}.jpg").is_file()
        )

    @staticmethod
    def _replace_directory(source, destination):
        parent = os.path.dirname(destination)
        temporary = tempfile.mkdtemp(
            prefix=f".{os.path.basename(destination)}.sync-", dir=parent
        )
        staged = os.path.join(temporary, os.path.basename(destination))
        backup = destination + ".previous"
        try:
            shutil.copytree(source, staged)
            if os.path.exists(backup):
                shutil.rmtree(backup)
            if os.path.exists(destination):
                os.replace(destination, backup)
            os.replace(staged, destination)
            if os.path.exists(backup):
                shutil.rmtree(backup)
        except Exception:
            if os.path.exists(backup) and not os.path.exists(destination):
                os.replace(backup, destination)
            raise
        finally:
            shutil.rmtree(temporary, ignore_errors=True)

    @staticmethod
    def _write_catalog(target):
        entries = sorted(
            path.name
            for path in Path(target).iterdir()
            if path.is_dir()
            and not path.name.startswith(".")
            and RockboxMagazineService._valid_prepared_issue(path)
        )
        temporary = os.path.join(target, "catalog.mgi.tmp")
        with open(temporary, "w", encoding="utf-8", newline="\n") as output:
            output.write("# Rockbox Magazines catalog v1\n")
            output.writelines(f"{entry}\n" for entry in entries)
        os.replace(temporary, os.path.join(target, "catalog.mgi"))
