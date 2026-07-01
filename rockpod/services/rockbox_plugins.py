"""Rockbox plugin discovery, metadata, and deployment bundles."""

from __future__ import annotations

import os


PLUGIN_OVERRIDES = {
    "runepod": {
        "display_name": "RunePod",
        "status": "experimental",
        "category": "games",
        "custom": True,
        "summary": "Native click-wheel fantasy RPG with persistent saves.",
        "docs": [
            "docs/runepod-rpg-expansion-spec.md",
        ],
        "dependencies": [
            ".rockbox/rocks/games/runepod/ generated sprite and tile sheets",
        ],
    },
    "pocketcatch": {
        "display_name": "Podemon Go",
        "status": "experimental",
        "category": "games",
        "custom": True,
        "summary": "Pokemon Go-style catch prototype for the click wheel.",
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
            source_abs, destination_rel = self._asset_source_destination(metadata, asset_path)
            if not os.path.isfile(source_abs):
                continue
            assets.append(
                {
                    "kind": "plugin_asset",
                    "source_rel": os.path.relpath(source_abs, metadata["repo_root"]).replace("\\", "/"),
                    "source_abs": source_abs,
                    "destination_rel": destination_rel,
                    "exists": True,
                    "size": os.path.getsize(source_abs),
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
            _source_abs, destination_rel = self._asset_source_destination(metadata, asset_path)
            assets.append(
                {
                    "kind": "plugin_asset",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": destination_rel,
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
        elif plugin_id == "pocketcatch":
            asset_sources = self._pocketcatch_asset_sources(repo_root, category, profile, simulator_target)
        elif plugin_id == "runepod":
            asset_sources = self._runepod_asset_sources(repo_root, category)
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
    def _asset_source_destination(metadata, asset_source):
        if isinstance(asset_source, dict):
            return asset_source["source_abs"], asset_source["destination_rel"]
        filename = os.path.basename(asset_source)
        return asset_source, f"{metadata['asset_destination_dir']}/{filename}"

    @staticmethod
    def _runepod_asset_sources(repo_root, category):
        asset_root = os.path.join(repo_root, "rockpod", "assets", "runepod")
        destination_root = f".rockbox/rocks/{category}/runepod"
        sources = []
        for rel_path in (
            "sprites/runepod_sprites.320x160x24.bmp",
            "sprites/runepod_player_dirs.384x32x24.bmp",
            "tiles/runepod_terrain_tiles.256x32x24.bmp",
        ):
            source_abs = os.path.join(asset_root, rel_path)
            if os.path.isfile(source_abs):
                sources.append(
                    {
                        "source_abs": source_abs,
                        "destination_rel": f"{destination_root}/{rel_path}",
                    }
                )
        return sources

    @classmethod
    def _pocketcatch_asset_sources(cls, repo_root, category, profile, simulator_target):
        pack_root = cls._pocketcatch_pack_root(repo_root, profile, simulator_target)
        if not pack_root:
            return []

        destination_root = f".rockbox/rocks/{category}/pocketcatch"
        sources = []
        rel_paths = set()
        for root, _dirs, files in os.walk(pack_root):
            for filename in sorted(files):
                source_abs = os.path.join(root, filename)
                rel_path = os.path.relpath(source_abs, pack_root).replace("\\", "/")
                rel_paths.add(rel_path)
                sources.append(
                    {
                        "source_abs": source_abs,
                        "destination_rel": f"{destination_root}/{rel_path}",
                    }
                )

        bg_alias = "backgrounds/scene_day_layer0.bmp"
        bg_source = os.path.join(pack_root, "backgrounds", "new_bark_town_hgss.bmp")
        if bg_alias not in rel_paths and os.path.isfile(bg_source):
            sources.append(
                {
                    "source_abs": bg_source,
                    "destination_rel": f"{destination_root}/{bg_alias}",
                }
            )
        return sources

    @staticmethod
    def _pocketcatch_pack_root(repo_root, profile, simulator_target):
        candidates = []
        for rel_path in (
            os.path.join("rockpod", "assets", "pocketcatch"),
            os.path.join("rockpod", "assets", "pocketcatch_personal"),
        ):
            candidates.append(os.path.join(repo_root, rel_path))

        resolution = str((profile or {}).get("screen_resolution") or "").strip()
        if resolution:
            candidates.append(
                os.path.join(
                    repo_root,
                    "rockpod",
                    ".theme_designer",
                    "simulator",
                    f"ipod-{resolution}",
                    "build-sim-video-5g",
                    "simdisk",
                    ".rockbox",
                    "rocks",
                    "games",
                    "pocketcatch",
                )
            )

        if simulator_target:
            simdisk_path = simulator_target.get("simdisk_path") or ""
            if simdisk_path:
                candidates.append(os.path.join(simdisk_path, ".rockbox", "rocks", "games", "pocketcatch"))

        theme_designer_root = os.path.join(repo_root, "rockpod", ".theme_designer", "simulator")
        if os.path.isdir(theme_designer_root):
            for sim_name in sorted(os.listdir(theme_designer_root)):
                sim_root = os.path.join(theme_designer_root, sim_name)
                if not os.path.isdir(sim_root):
                    continue
                for build_name in sorted(os.listdir(sim_root)):
                    candidates.append(
                        os.path.join(
                            sim_root,
                            build_name,
                            "simdisk",
                            ".rockbox",
                            "rocks",
                            "games",
                            "pocketcatch",
                        )
                    )

        for candidate in candidates:
            if RockboxPluginService._valid_pocketcatch_pack_root(candidate):
                return candidate
        return ""

    @staticmethod
    def _valid_pocketcatch_pack_root(candidate):
        if not candidate or not os.path.isdir(candidate):
            return False
        required = (
            "pack.json",
            os.path.join("sprites", "creatures", "creature_001_idle_0.bmp"),
            os.path.join("sprites", "balls", "ball_default_idle_0.bmp"),
        )
        return all(os.path.isfile(os.path.join(candidate, rel_path)) for rel_path in required)

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
        if "ipod 3g" in model or resolution == "160x128":
            preferred = ["build-hw-ipod3g", "build-hw-ipodvideo", "build-hw-ipodvideo-5g"]
        elif "nano" in model or resolution == "176x132":
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
