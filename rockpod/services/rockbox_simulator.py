"""Rockbox simulator discovery, binding, launch, and screenshot support."""

from __future__ import annotations

import glob
import os
import shutil
import subprocess
import time
from datetime import datetime


_PREVIEW_AUDIO_EXTENSIONS = (".flac", ".mp3", ".ogg", ".wav", ".m4a", ".aac", ".opus")


def _guess_resolution(name):
    lowered = str(name or "").lower()
    if "nano2g" in lowered:
        return "176x132"
    return "320x240"


class RockboxSimulatorService:
    """Discover and operate on in-tree Rockbox simulator targets."""

    def discover_targets(self, repo_root):
        repo_root = os.path.abspath(repo_root)
        targets = []
        for path in sorted(glob.glob(os.path.join(repo_root, "build-sim*"))):
            if not os.path.isdir(path):
                continue
            binary = os.path.join(path, "rockboxui")
            simdisk = os.path.join(path, "simdisk")
            if not (os.path.isfile(binary) and os.path.isdir(simdisk)):
                continue
            target_id = os.path.basename(path)
            simdisk_rb = os.path.join(simdisk, ".rockbox")
            targets.append(
                {
                    "id": target_id,
                    "name": target_id,
                    "build_dir": path,
                    "binary_path": binary,
                    "simdisk_path": simdisk,
                    "rockbox_root": simdisk_rb,
                    "screen_resolution": _guess_resolution(target_id),
                    "device_model": "iPod nano 2G" if "nano2g" in target_id.lower() else "iPod Classic / Video",
                }
            )
        return targets

    def bind_profile(self, profile, target, screenshot_dir=""):
        updated = dict(profile)
        updated["simulator_target"] = target["id"]
        updated["simulator_binary_path"] = target["binary_path"]
        updated["simulator_simdisk_path"] = target["simdisk_path"]
        updated["simulator_screenshot_dir"] = os.path.abspath(
            screenshot_dir or profile.get("simulator_screenshot_dir") or os.path.join(profile["source_repo_path"], "simshots")
        )
        return updated

    def simulator_profile(self, profile, target):
        simdisk_path = profile.get("simulator_simdisk_path") or target["simdisk_path"]
        return {
            "id": f"{profile['id']}-sim",
            "name": f"{profile['name']} Simulator",
            "device_mount_path": os.path.abspath(simdisk_path),
            "target_device_model": profile.get("target_device_model") or target["device_model"],
            "screen_resolution": profile.get("screen_resolution") or target["screen_resolution"],
            "source_repo_path": profile["source_repo_path"],
            "selected_theme": profile["selected_theme"],
            "backup_location": os.path.abspath(
                os.path.join(profile["source_repo_path"], "rockpod", ".backups", "simulator", profile["id"])
            ),
        }

    def screenshot_dir(self, profile, target=None):
        root = profile.get("simulator_screenshot_dir") or os.path.join(profile["source_repo_path"], "simshots")
        target_name = profile.get("simulator_target") or (target["id"] if target else "simulator")
        return os.path.abspath(os.path.join(root, target_name))

    def ensure_screenshot_dir(self, profile, target=None):
        path = self.screenshot_dir(profile, target)
        os.makedirs(path, exist_ok=True)
        return path

    def theme_designer_preview_target(self, profile, target, reset=False):
        repo_root = os.path.abspath(profile["source_repo_path"])
        preview_root = os.path.join(repo_root, "rockpod", ".theme_designer", "simulator", profile["id"], target["id"])
        simdisk = os.path.join(preview_root, "simdisk")
        if reset and os.path.isdir(preview_root):
            self._reset_preview_root(preview_root)
        if not os.path.isdir(simdisk):
            os.makedirs(preview_root, exist_ok=True)
            shutil.copytree(target["simdisk_path"], simdisk, dirs_exist_ok=True)
        preview = dict(target)
        base_name = target.get("name") or target.get("id") or "simulator"
        preview["id"] = f"{target['id']}-theme-designer"
        preview["name"] = f"{base_name} Theme Designer"
        preview["simdisk_path"] = simdisk
        preview["rockbox_root"] = os.path.join(simdisk, ".rockbox")
        preview["preview_root"] = preview_root
        return preview

    def latest_screenshot(self, profile, target=None):
        if target:
            preview_root = str(target.get("preview_root") or "").strip()
            if preview_root:
                capture_dir = os.path.join(preview_root, "captures")
                if os.path.isdir(capture_dir):
                    captures = [
                        os.path.join(capture_dir, name)
                        for name in os.listdir(capture_dir)
                        if name.lower().endswith(".bmp")
                        and os.path.isfile(os.path.join(capture_dir, name))
                    ]
                    if captures:
                        return max(captures, key=os.path.getmtime)
            live_preview = os.path.join(target.get("simdisk_path", ""), ".rockbox", "live_preview.bmp")
            if os.path.isfile(live_preview):
                return live_preview
            if str(target.get("id") or "").endswith("-theme-designer"):
                return ""
        if target and not str(target.get("id") or "").endswith("-theme-designer"):
            for name in ("1UI256.bmp", "UI256.bmp"):
                candidate = os.path.join(target.get("build_dir", ""), name)
                if os.path.isfile(candidate):
                    return candidate
        screenshot_dir = self.screenshot_dir(profile, target)
        if not os.path.isdir(screenshot_dir):
            return ""
        candidates = []
        for name in os.listdir(screenshot_dir):
            full = os.path.join(screenshot_dir, name)
            if not os.path.isfile(full):
                continue
            if not name.lower().endswith(".bmp"):
                continue
            candidates.append(full)
        if not candidates:
            return ""
        return max(candidates, key=os.path.getmtime)

    def activate_theme_preview(self, target, theme_id, preview_screen="wps"):
        simdisk_path = os.path.abspath(target.get("simdisk_path") or "")
        preview_root = os.path.abspath(target.get("preview_root") or os.path.dirname(simdisk_path))
        theme_name = str(theme_id or "").strip()
        preview_screen = str(preview_screen or "wps").strip().lower() or "wps"
        if not simdisk_path or not theme_name:
            return False

        theme_cfg = os.path.join(simdisk_path, ".rockbox", "themes", f"{theme_name}.cfg")
        if not os.path.isfile(theme_cfg):
            return False

        settings = self._read_cfg_settings(theme_cfg)
        if not settings:
            return False
        settings["theme"] = f"/.rockbox/themes/{theme_name}.cfg"
        wants_playback = preview_screen in {"wps", "lockscreen"}
        settings["start in screen"] = "wps" if wants_playback else "root"
        if wants_playback:
            settings["repeat"] = "all"

        updated = False
        config_paths = (
            os.path.join(preview_root, ".config", "rockbox.org", "config.cfg"),
            os.path.join(simdisk_path, ".rockbox", "config.cfg"),
            os.path.join(simdisk_path, "config.cfg"),
        )
        for config_path in config_paths:
            self._merge_cfg_settings(config_path, settings)
            updated = True

        resume_paths = (
            os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg"),
            os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.new"),
            os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.old"),
            os.path.join(simdisk_path, ".rockbox", ".resume.cfg"),
            os.path.join(simdisk_path, ".rockbox", ".resume.cfg.new"),
            os.path.join(simdisk_path, ".rockbox", ".resume.cfg.old"),
            os.path.join(simdisk_path, ".resume.cfg"),
            os.path.join(simdisk_path, ".resume.cfg.new"),
            os.path.join(simdisk_path, ".resume.cfg.old"),
        )
        for resume_path in resume_paths:
            try:
                if os.path.exists(resume_path):
                    os.remove(resume_path)
            except OSError:
                pass

        playlist_paths = (
            os.path.join(preview_root, ".config", "rockbox.org", ".playlist_control"),
            os.path.join(simdisk_path, ".rockbox", ".playlist_control"),
            os.path.join(simdisk_path, ".playlist_control"),
        )
        for playlist_path in playlist_paths:
            try:
                if os.path.exists(playlist_path):
                    os.remove(playlist_path)
            except OSError:
                pass

        if wants_playback:
            track_path = self._first_preview_track(simdisk_path)
            if track_path:
                self._write_preview_playlist(playlist_paths[1], track_path)
                self._write_preview_resume(os.path.join(simdisk_path, ".rockbox", ".resume.cfg"))
                self._write_preview_resume(os.path.join(simdisk_path, ".rockbox", ".resume.cfg.new"))
                self._write_preview_resume(os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg"))
                self._write_preview_resume(os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.new"))
        return updated

    @staticmethod
    def _read_cfg_settings(path):
        settings = {}
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                for raw in handle:
                    line = raw.strip()
                    if not line or line.startswith("#") or ":" not in line:
                        continue
                    key, value = line.split(":", 1)
                    settings[key.strip().lower()] = value.strip()
        except OSError:
            return {}
        return settings

    @staticmethod
    def _merge_cfg_settings(path, overrides):
        lines = []
        try:
            if os.path.exists(path):
                with open(path, "r", encoding="utf-8", errors="replace") as handle:
                    lines = handle.readlines()
        except OSError:
            lines = []

        remaining = {str(key).strip().lower(): str(value).strip() for key, value in (overrides or {}).items() if str(key).strip()}
        output = []
        for raw in lines:
            stripped = raw.strip()
            if not stripped or stripped.startswith("#") or ":" not in stripped:
                output.append(raw)
                continue
            key, _value = stripped.split(":", 1)
            normalized = key.strip().lower()
            if normalized in remaining:
                output.append(f"{key.strip()}: {remaining.pop(normalized)}\n")
            else:
                output.append(raw)

        for key, value in remaining.items():
            output.append(f"{key}: {value}\n")

        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            handle.writelines(output)

    def launch(self, target, detached=True):
        cmd = [target["binary_path"], "--nobackground", "--root", target["simdisk_path"]]
        if "video" in target["id"]:
            cmd.extend(["--zoom", "2"])
        kwargs = {
            "cwd": target["build_dir"],
            "stdout": subprocess.DEVNULL,
            "stderr": subprocess.DEVNULL,
            "start_new_session": detached,
        }
        process = subprocess.Popen(cmd, **kwargs)
        return {"pid": process.pid, "command": cmd, "build_dir": target["build_dir"]}

    def launch_with_rom(self, target, rom_path=None, detached=True):
        launched = self.launch(target, detached=detached)
        launched["rom_path"] = rom_path or ""
        launched["autoload_supported"] = False
        launched["message"] = (
            f"Simulator launched with ROM staged at {rom_path}"
            if rom_path
            else "Simulator launched"
        )
        return launched

    def capture_theme_preview(self, target, preview_screen="wps", timeout=6.0):
        binary_path = str(target.get("binary_path") or "").strip()
        simdisk_path = os.path.abspath(target.get("simdisk_path") or "")
        build_dir = os.path.abspath(target.get("build_dir") or os.path.dirname(binary_path))
        preview_root = os.path.abspath(target.get("preview_root") or os.path.dirname(simdisk_path))
        preview_screen = str(preview_screen or "wps").strip().lower() or "wps"
        if not binary_path or not os.path.isfile(binary_path) or not simdisk_path or not os.path.isdir(simdisk_path):
            return ""

        host_capture = os.path.join(simdisk_path, ".rockbox", "live_preview.bmp")
        try:
            if os.path.exists(host_capture):
                os.remove(host_capture)
        except OSError:
            pass

        env = os.environ.copy()
        env["RBROOT"] = preview_root
        env["ROCKPOD_SIM_PREVIEW_BMP"] = "/.rockbox/live_preview.bmp"
        env["ROCKPOD_SIM_PREVIEW_INTERVAL_MS"] = "5000"
        if shutil.which("xdotool") is None:
            env["ROCKPOD_SIM_HIDDEN"] = "1"

        process = subprocess.Popen(
            [binary_path, "--nobackground", "--root", simdisk_path],
            cwd=build_dir,
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        try:
            window_id = self._wait_for_window_id(process.pid, timeout=3.0)
            if window_id:
                self._move_window_offscreen(window_id)
            time.sleep(1.8)
            if preview_screen == "lockscreen":
                self._send_key_to_window_id(window_id, "h")
                time.sleep(0.8)
            try:
                if os.path.exists(host_capture):
                    os.remove(host_capture)
            except OSError:
                pass
            self._send_key_to_window_id(window_id, "F5")
            deadline = time.monotonic() + max(0.5, float(timeout))
            while time.monotonic() < deadline:
                if os.path.isfile(host_capture) and os.path.getsize(host_capture) > 0:
                    return self._snapshot_preview_capture(target, host_capture)
                if process.poll() is not None:
                    break
                time.sleep(0.1)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=1.0)
        if os.path.isfile(host_capture):
            return self._snapshot_preview_capture(target, host_capture)
        return ""

    def capture_screenshot(self, profile, target):
        screenshot_dir = self.ensure_screenshot_dir(profile, target)
        candidates = []
        for pattern in (
            os.path.join(target["simdisk_path"], "dump *.bmp"),
            os.path.join(target["simdisk_path"], "dump_*.bmp"),
            os.path.join(target["build_dir"], "dump *.bmp"),
            os.path.join(target["build_dir"], "dump_*.bmp"),
        ):
            candidates.extend(glob.glob(pattern))
        candidates = [path for path in candidates if os.path.isfile(path)]
        if not candidates:
            return {"success": False, "message": "No simulator screendump found", "captured_path": ""}
        latest = max(candidates, key=os.path.getmtime)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        base = os.path.basename(latest).replace(" ", "_")
        dest = os.path.join(screenshot_dir, f"{stamp}-{base}")
        shutil.copy2(latest, dest)
        return {"success": True, "message": "Screenshot captured", "captured_path": dest, "source_path": latest}

    @staticmethod
    def _reset_preview_root(preview_root):
        preview_root = os.path.abspath(preview_root)
        shutil.rmtree(preview_root, ignore_errors=True)
        if not os.path.exists(preview_root):
            return

        stale_root = f"{preview_root}.stale-{datetime.now().strftime('%Y%m%d-%H%M%S-%f')}"
        try:
            os.replace(preview_root, stale_root)
        except OSError:
            simdisk = os.path.join(preview_root, "simdisk")
            shutil.rmtree(simdisk, ignore_errors=True)
            if os.path.isdir(preview_root):
                for entry in os.listdir(preview_root):
                    path = os.path.join(preview_root, entry)
                    if os.path.isdir(path):
                        shutil.rmtree(path, ignore_errors=True)
                    else:
                        try:
                            os.remove(path)
                        except OSError:
                            pass
            return

        shutil.rmtree(stale_root, ignore_errors=True)

    @staticmethod
    def _wait_for_window_and_send_key(pid, key_name, timeout=2.5):
        window_id = RockboxSimulatorService._wait_for_window_id(pid, timeout=timeout)
        return RockboxSimulatorService._send_key_to_window_id(window_id, key_name)

    @staticmethod
    def _wait_for_window_id(pid, timeout=2.5):
        if not pid or shutil.which("xdotool") is None:
            return ""
        deadline = time.monotonic() + max(0.2, float(timeout))
        while time.monotonic() < deadline:
            try:
                result = subprocess.run(
                    ["xdotool", "search", "--pid", str(pid)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL,
                    text=True,
                    timeout=1.0,
                    check=False,
                )
            except (OSError, subprocess.TimeoutExpired):
                result = None
            ids = [line.strip() for line in (result.stdout.splitlines() if result else []) if line.strip().isdigit()]
            if ids:
                return ids[-1]
            time.sleep(0.1)
        return ""

    @staticmethod
    def _send_key_to_window_id(window_id, key_name):
        if not window_id or shutil.which("xdotool") is None:
            return False
        try:
            sent = subprocess.run(
                ["xdotool", "key", "--window", window_id, key_name],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=1.0,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            return False
        return sent.returncode == 0

    @staticmethod
    def _move_window_offscreen(window_id):
        if not window_id or shutil.which("xdotool") is None:
            return False
        try:
            moved = subprocess.run(
                ["xdotool", "windowmove", window_id, "5000", "5000"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=1.0,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            return False
        return moved.returncode == 0

    @staticmethod
    def _first_preview_track(simdisk_path):
        for root, _dirs, files in os.walk(simdisk_path):
            for name in sorted(files):
                if not name.lower().endswith(_PREVIEW_AUDIO_EXTENSIONS):
                    continue
                full = os.path.join(root, name)
                rel = os.path.relpath(full, simdisk_path).replace("\\", "/")
                return f"/{rel}"
        return ""

    @staticmethod
    def _write_preview_playlist(path, track_path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(f"P:6::\nA:0:0:{track_path}\n")

    @staticmethod
    def _write_preview_resume(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(
                "volume: 6\n"
                "pitch: 10000\n"
                "speed: 10000\n"
                "IDX: 0\n"
                "CRC: 0\n"
                "ELA: 0\n"
                "OFF: 0\n"
                "PLM: 0\n"
                "CRT: 1\n"
                "TRT: 1\n"
                "PVS: -1\n"
                "PFQ: 0\n"
            )

    @staticmethod
    def _snapshot_preview_capture(target, source_path):
        source_path = os.path.abspath(source_path)
        if not os.path.isfile(source_path):
            return ""
        preview_root = os.path.abspath(target.get("preview_root") or os.path.dirname(os.path.dirname(source_path)))
        snapshot_dir = os.path.join(preview_root, "captures")
        os.makedirs(snapshot_dir, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        snapshot_path = os.path.join(snapshot_dir, f"preview-{stamp}.bmp")
        shutil.copy2(source_path, snapshot_path)
        try:
            captures = sorted(
                (
                    os.path.join(snapshot_dir, name)
                    for name in os.listdir(snapshot_dir)
                    if name.startswith("preview-") and name.lower().endswith(".bmp")
                ),
                key=os.path.getmtime,
                reverse=True,
            )
            for stale in captures[6:]:
                try:
                    os.remove(stale)
                except OSError:
                    pass
        except OSError:
            pass
        return snapshot_path
