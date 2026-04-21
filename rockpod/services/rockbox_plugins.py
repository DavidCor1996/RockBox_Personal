"""Rockbox plugin discovery, metadata, and deployment bundles."""

from __future__ import annotations

import os


PLUGIN_OVERRIDES = {
    "pocketcatch": {
        "display_name": "PocketCatch",
        "status": "experimental",
        "category": "games",
        "custom": True,
        "summary": "Monster-catching prototype with external pack support.",
        "docs": [
            "apps/plugins/POCKETCATCH_PLAN.md",
            "apps/plugins/POCKETCATCH_ASSET_PACK_SPEC.md",
        ],
        "dependencies": [
            ".rockbox/rocks/games/pocketcatch/ (optional external asset pack)",
        ],
    },
    "pocketcatch_simple": {
        "display_name": "PocketCatch Simple",
        "status": "stable",
        "category": "games",
        "custom": True,
        "summary": "Minimal fallback build for PocketCatch workflows.",
        "docs": [],
        "dependencies": [],
    },
    "minishcap": {
        "display_name": "Minish Cap",
        "status": "experimental",
        "category": "games",
        "custom": True,
        "summary": "Custom Zelda-inspired plugin with runtime-loaded art assets.",
        "docs": [
            "apps/plugins/MINISHCAP_USED_ASSETS.md",
        ],
        "dependencies": [
            "minishcap_smith_real.bmp",
            "minishcap_zelda_real.bmp",
            "minishcap_south_hyrule_full.1008x688x24.bmp",
        ],
    },
}


class RockboxPluginService:
    """Index repo plugins and build deploy/remove bundles."""

    def list_plugins(self, repo_root, profile, simulator_target=None, target_mode="device"):
        repo_root = os.path.abspath(repo_root)
        categories = self._load_categories(repo_root)
        plugins = []
        for plugin_id, category in sorted(categories.items()):
            source_path = self._source_path(repo_root, plugin_id)
            if not source_path:
                continue
            metadata = self._metadata(repo_root, plugin_id, category, profile, simulator_target, target_mode)
            plugins.append(metadata)
        plugins.sort(key=lambda item: (not item["custom"], item["display_name"].lower()))
        return plugins

    def plugin_details(self, repo_root, plugin_id, profile, simulator_target=None, target_mode="device"):
        categories = self._load_categories(repo_root)
        category = categories.get(plugin_id)
        if not category:
            raise KeyError(plugin_id)
        return self._metadata(repo_root, plugin_id, category, profile, simulator_target, target_mode)

    def deploy_profile(self, profile, target_mode="device", simulator_target=None):
        mount_path = profile.get("device_mount_path") or ""
        backup_root = os.path.join(profile["source_repo_path"], "rockpod", ".backups", "plugins", profile["id"], target_mode)
        if target_mode == "simulator":
            mount_path = (
                profile.get("simulator_simdisk_path")
                or (simulator_target or {}).get("simdisk_path")
                or ""
            )
        return {
            "id": f"{profile['id']}-plugins-{target_mode}",
            "name": f"{profile['name']} Plugins ({target_mode})",
            "device_mount_path": os.path.abspath(mount_path) if mount_path else "",
            "target_device_model": profile.get("target_device_model", ""),
            "screen_resolution": profile.get("screen_resolution", ""),
            "source_repo_path": profile["source_repo_path"],
            "selected_theme": profile.get("selected_theme", ""),
            "backup_location": os.path.abspath(backup_root),
        }

    def build_deploy_bundle(self, metadata):
        assets = []
        binary_path = metadata.get("binary_path", "")
        if binary_path and os.path.isfile(binary_path):
            assets.append(
                {
                    "kind": "plugin_binary",
                    "source_rel": os.path.relpath(binary_path, metadata["repo_root"]).replace("\\", "/"),
                    "source_abs": binary_path,
                    "destination_rel": metadata["destination_rel"],
                    "exists": True,
                    "size": os.path.getsize(binary_path),
                }
            )
        for asset_path in metadata.get("asset_source_paths", []):
            if not os.path.isfile(asset_path):
                continue
            filename = os.path.basename(asset_path)
            assets.append(
                {
                    "kind": "plugin_asset",
                    "source_rel": os.path.relpath(asset_path, metadata["repo_root"]).replace("\\", "/"),
                    "source_abs": asset_path,
                    "destination_rel": f"{metadata['asset_destination_dir']}/{filename}",
                    "exists": True,
                    "size": os.path.getsize(asset_path),
                }
            )
        return {
            "id": f"plugin-{metadata['id']}",
            "name": metadata["display_name"],
            "assets": assets,
        }

    def build_remove_bundle(self, metadata):
        assets = [
            {
                "kind": "plugin_binary",
                "source_rel": "",
                "source_abs": "",
                "destination_rel": metadata["destination_rel"],
                "exists": False,
                "action": "remove",
            }
        ]
        for asset_path in metadata.get("asset_source_paths", []):
            filename = os.path.basename(asset_path)
            assets.append(
                {
                    "kind": "plugin_asset",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": f"{metadata['asset_destination_dir']}/{filename}",
                    "exists": False,
                    "action": "remove",
                }
            )
        return {
            "id": f"plugin-remove-{metadata['id']}",
            "name": f"Remove {metadata['display_name']}",
            "assets": assets,
        }

    def _metadata(self, repo_root, plugin_id, category, profile, simulator_target, target_mode):
        override = PLUGIN_OVERRIDES.get(plugin_id, {})
        build_dir = self._build_dir(repo_root, profile, simulator_target, target_mode)
        binary_path = os.path.join(build_dir, "apps", "plugins", f"{plugin_id}.rock") if build_dir else ""
        destination_rel = f".rockbox/rocks/{category}/{plugin_id}.rock"
        asset_sources = []
        if plugin_id == "minishcap":
            for filename in (
                "minishcap_smith_real.bmp",
                "minishcap_zelda_real.bmp",
                "minishcap_south_hyrule_full.1008x688x24.bmp",
            ):
                candidate = os.path.join(repo_root, "apps", "plugins", filename)
                if os.path.isfile(candidate):
                    asset_sources.append(candidate)
        return {
            "id": plugin_id,
            "display_name": override.get("display_name", plugin_id.replace("_", " ").title()),
            "category": override.get("category", category),
            "status": override.get("status", "stable"),
            "custom": bool(override.get("custom")),
            "source_path": self._source_path(repo_root, plugin_id) or "",
            "binary_path": binary_path if os.path.isfile(binary_path) else "",
            "binary_exists": os.path.isfile(binary_path),
            "destination_rel": destination_rel,
            "asset_destination_dir": f".rockbox/rocks/{category}",
            "asset_source_paths": asset_sources,
            "dependencies": list(override.get("dependencies", [])),
            "docs": [os.path.join(repo_root, path) for path in override.get("docs", [])],
            "summary": override.get("summary", ""),
            "simulator_supported": bool(
                simulator_target and os.path.isfile(os.path.join(simulator_target["build_dir"], "apps", "plugins", f"{plugin_id}.rock"))
            ),
            "repo_root": repo_root,
        }

    @staticmethod
    def _load_categories(repo_root):
        categories = {}
        path = os.path.join(repo_root, "apps", "plugins", "CATEGORIES")
        with open(path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [part.strip() for part in line.split(",", 1)]
                if len(parts) != 2:
                    continue
                plugin_id = parts[0]
                categories[plugin_id.lower()] = parts[1]
        return categories

    @staticmethod
    def _source_path(repo_root, plugin_id):
        plugins_root = os.path.join(repo_root, "apps", "plugins")
        file_path = os.path.join(plugins_root, f"{plugin_id}.c")
        dir_path = os.path.join(plugins_root, plugin_id)
        if os.path.isfile(file_path):
            return file_path
        if os.path.isdir(dir_path):
            return dir_path
        return ""

    @staticmethod
    def _build_dir(repo_root, profile, simulator_target, target_mode):
        if target_mode == "simulator":
            return (simulator_target or {}).get("build_dir", "")
        model = str(profile.get("target_device_model", "")).lower()
        resolution = str(profile.get("screen_resolution", "")).strip()
        if "nano" in model or resolution == "176x132":
            preferred = ["build-hw-ipodnano2g"]
        elif "video" in model or "5g" in model:
            preferred = ["build-hw-ipodvideo-5g", "build-hw-ipodvideo"]
        else:
            preferred = ["build-hw-ipod6g", "build-hw-ipodvideo-5g", "build-hw-ipodvideo"]
        for name in preferred:
            candidate = os.path.join(repo_root, name)
            if os.path.isdir(candidate):
                return candidate
        return ""
