"""Rockbox theme deployment and rollback.

This is the only RockPod service that performs Rockbox file writes.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
from datetime import datetime

from services.file_safety import atomic_write_json
from services.path_safety import resolve_under_root, validate_device_root


class RockboxDeployService:
    """Build diffs, apply theme bundles, and restore backups."""

    def build_diff(self, profile, theme_bundle):
        self._validate_theme_compatibility(profile, theme_bundle)
        mount_path = validate_device_root(profile.get("device_mount_path") or "")

        items = []
        counts = {"add": 0, "overwrite": 0, "unchanged": 0, "missing_source": 0, "remove": 0}
        for asset in theme_bundle.get("assets", []):
            dest_full = resolve_under_root(mount_path, asset["destination_rel"])
            exists_on_device = os.path.exists(dest_full)
            action = asset.get("action") or "copy"
            source_exists = bool(asset.get("exists"))
            if action == "remove":
                status = "remove" if exists_on_device else "unchanged"
            elif not source_exists:
                status = "missing_source"
            elif not exists_on_device:
                status = "add"
            elif self._same_file(asset["source_abs"], dest_full):
                status = "unchanged"
            else:
                status = "overwrite"
            counts[status] = counts.get(status, 0) + 1
            items.append(
                {
                    "action": action,
                    "kind": asset["kind"],
                    "source_rel": asset["source_rel"],
                    "source_abs": asset.get("source_abs", ""),
                    "destination_rel": asset["destination_rel"],
                    "destination_abs": dest_full,
                    "status": status,
                    "source_exists": source_exists,
                    "destination_exists": exists_on_device,
                    "preserve_metadata": bool(asset.get("preserve_metadata", True)),
                }
            )

        return {
            "profile_id": profile["id"],
            "theme_id": theme_bundle["id"],
            "mount_path": mount_path,
            "items": items,
            "counts": counts,
            "summary": {
                "add": counts.get("add", 0),
                "overwrite": counts.get("overwrite", 0),
                "unchanged": counts.get("unchanged", 0),
                "missing_source": counts.get("missing_source", 0),
                "remove": counts.get("remove", 0),
            },
        }

    def apply_diff(self, profile, diff_report):
        mount_path = validate_device_root(diff_report.get("mount_path") or profile.get("device_mount_path") or "")
        actionable = [
            item for item in diff_report["items"]
            if item["status"] in ("add", "overwrite", "remove")
        ]
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        backup_root = os.path.abspath(profile.get("backup_location") or "")
        backup_dir = os.path.join(backup_root, timestamp)
        os.makedirs(backup_dir, exist_ok=True)

        manifest = {
            "profile_id": profile["id"],
            "theme_id": diff_report["theme_id"],
            "created_at": datetime.now().isoformat(timespec="seconds"),
            "mount_path": diff_report["mount_path"],
            "items": [],
        }
        copied = 0
        failures = []
        for item in actionable:
            dest_abs = item["destination_abs"]
            if not self._within_mount(mount_path, dest_abs):
                failures.append(f"{item['destination_rel']}: destination escapes device root")
                continue
            existed = os.path.exists(dest_abs)
            backup_rel = item["destination_rel"].lstrip("/")
            try:
                backup_abs = resolve_under_root(backup_dir, backup_rel)
            except ValueError as exc:
                failures.append(f"{item['destination_rel']}: {exc}")
                continue
            if existed:
                os.makedirs(os.path.dirname(backup_abs), exist_ok=True)
                shutil.copy2(dest_abs, backup_abs)
            manifest["items"].append(
                {
                    "destination_rel": item["destination_rel"],
                    "destination_abs": dest_abs,
                    "backup_rel": backup_rel,
                    "existed": existed,
                }
            )
            try:
                if item.get("action") == "remove":
                    if os.path.exists(dest_abs):
                        os.remove(dest_abs)
                else:
                    os.makedirs(os.path.dirname(dest_abs), exist_ok=True)
                    if item.get("preserve_metadata", True):
                        shutil.copy2(item["source_abs"], dest_abs)
                    else:
                        shutil.copyfile(item["source_abs"], dest_abs)
                        os.utime(dest_abs, None)
                    if not self._same_file(item["source_abs"], dest_abs):
                        raise OSError("verification mismatch after copy")
                copied += 1
            except OSError as exc:
                failures.append(f"{item['destination_rel']}: {exc}")

        manifest_path = os.path.join(backup_dir, "manifest.json")
        atomic_write_json(manifest_path, manifest)

        return {
            "success": not failures,
            "copied_count": copied,
            "failure_count": len(failures),
            "failures": failures,
            "backup_dir": backup_dir,
            "manifest_path": manifest_path,
            "rollback_available": bool(manifest["items"]),
        }

    def restore_latest_backup(self, profile):
        mount_path = validate_device_root(profile.get("device_mount_path") or "")
        backup_root = os.path.abspath(profile.get("backup_location") or "")
        if not os.path.isdir(backup_root):
            return {"success": False, "restored_count": 0, "failures": ["No backup directory found"]}

        candidates = []
        for name in sorted(os.listdir(backup_root), reverse=True):
            manifest_path = os.path.join(backup_root, name, "manifest.json")
            if os.path.isfile(manifest_path):
                candidates.append(manifest_path)
        if not candidates:
            return {"success": False, "restored_count": 0, "failures": ["No backup manifest found"]}

        manifest_path = candidates[0]
        with open(manifest_path, "r", encoding="utf-8") as handle:
            manifest = json.load(handle)

        restored = 0
        failures = []
        backup_dir = os.path.dirname(manifest_path)
        for item in manifest.get("items", []):
            dest_abs = item["destination_abs"]
            if not self._within_mount(mount_path, dest_abs):
                failures.append(f"{item.get('destination_rel', dest_abs)}: destination escapes device root")
                continue
            try:
                backup_abs = resolve_under_root(backup_dir, item["backup_rel"])
            except ValueError as exc:
                failures.append(f"{item.get('destination_rel', dest_abs)}: {exc}")
                continue
            try:
                if item.get("existed"):
                    os.makedirs(os.path.dirname(dest_abs), exist_ok=True)
                    shutil.copy2(backup_abs, dest_abs)
                elif os.path.exists(dest_abs):
                    os.remove(dest_abs)
                restored += 1
            except OSError as exc:
                failures.append(f"{item['destination_rel']}: {exc}")

        return {
            "success": not failures,
            "restored_count": restored,
            "failure_count": len(failures),
            "failures": failures,
            "backup_dir": backup_dir,
            "manifest_path": manifest_path,
        }

    @staticmethod
    def _same_file(left, right):
        if not (os.path.isfile(left) and os.path.isfile(right)):
            return False
        return RockboxDeployService._hash_file(left) == RockboxDeployService._hash_file(right)

    @staticmethod
    def _hash_file(path):
        digest = hashlib.sha256()
        with open(path, "rb") as handle:
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
        return digest.hexdigest()

    @staticmethod
    def _within_mount(mount_path, path):
        try:
            return os.path.commonpath([os.path.abspath(mount_path), os.path.abspath(path)]) == os.path.abspath(mount_path)
        except ValueError:
            return False

    @staticmethod
    def _validate_theme_compatibility(profile, theme_bundle):
        compatible = [
            str(item or "").strip().lower()
            for item in theme_bundle.get("compatible_device_models", [])
        ]
        compatible = [item for item in compatible if item]
        if not compatible:
            return
        model = str(profile.get("target_device_model") or "").strip().lower()
        if model and any(item in model for item in compatible):
            return
        theme_id = theme_bundle.get("id") or "theme"
        allowed = ", ".join(theme_bundle.get("compatible_device_models", []))
        raise ValueError(f"{theme_id} can only be deployed to: {allowed}")
