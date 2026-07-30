"""Build and invoke the exact portable Maker Lite C core for Rockpod tests."""

from __future__ import annotations

import ctypes
import os
import subprocess
import tempfile
from pathlib import Path

from services.maker_lite_pack import compile_project


class MakerLiteRuntimeError(RuntimeError):
    pass


class _Snapshot(ctypes.Structure):
    _fields_ = [
        ("tick", ctypes.c_uint32),
        ("player_x", ctypes.c_int32),
        ("player_y", ctypes.c_int32),
        ("player_vx", ctypes.c_int32),
        ("player_vy", ctypes.c_int32),
        ("camera_x", ctypes.c_int32),
        ("camera_y", ctypes.c_int32),
        ("collectibles", ctypes.c_int16),
        ("rings", ctypes.c_int16),
        ("keys", ctypes.c_int16),
        ("health", ctypes.c_int16),
        ("action", ctypes.c_uint8),
        ("grounded", ctypes.c_uint8),
        ("complete", ctypes.c_uint8),
        ("paused", ctypes.c_uint8),
        ("facing_x", ctypes.c_int16),
        ("facing_y", ctypes.c_int16),
        ("power_state", ctypes.c_uint8),
        ("carrying", ctypes.c_uint8),
        ("shield", ctypes.c_uint8),
        ("reserved", ctypes.c_uint8),
        ("credits", ctypes.c_uint32),
        ("debt", ctypes.c_uint32),
        ("house_level", ctypes.c_uint16),
        ("car_level", ctypes.c_uint16),
        ("job_cooldown", ctypes.c_uint16),
        ("furniture_held_entity", ctypes.c_uint16),
        ("house_style", ctypes.c_uint8),
        ("car_active", ctypes.c_uint8),
        ("interaction", ctypes.c_uint8),
        ("interaction_choice", ctypes.c_uint8),
        ("interaction_notice", ctypes.c_uint8),
        ("life_reserved", ctypes.c_uint8 * 3),
        ("brawl_player_damage", ctypes.c_int16),
        ("brawl_opponent_damage", ctypes.c_int16),
        ("brawl_opponent_entity", ctypes.c_uint16),
        ("brawl_player_stocks", ctypes.c_uint8),
        ("brawl_opponent_stocks", ctypes.c_uint8),
        ("brawl_reserved", ctypes.c_uint8 * 2),
    ]


class MakerLiteSession:
    FIXED_ONE = 1 << 16

    def __init__(self, library, pack: bytes):
        self.library = library
        self.pack = ctypes.create_string_buffer(pack)
        result = self.library.ml_bridge_open(self.pack, len(pack))
        if result:
            raise MakerLiteRuntimeError(f"shared core rejected pack ({result})")

    def reset(self):
        if self.library.ml_bridge_reset():
            raise MakerLiteRuntimeError("shared core reset failed")

    def tick(self, input_mask: int):
        if self.library.ml_bridge_tick(ctypes.c_uint32(input_mask)):
            raise MakerLiteRuntimeError("shared core tick failed")

    def snapshot(self) -> dict:
        value = _Snapshot()
        if self.library.ml_bridge_snapshot(ctypes.byref(value)):
            raise MakerLiteRuntimeError("shared core snapshot failed")
        result = {}
        for name, field_type in _Snapshot._fields_:
            field = getattr(value, name)
            result[name] = (
                tuple(field)
                if isinstance(field, ctypes.Array)
                else field
            )
        return result

    def counts(self) -> tuple[int, int, int]:
        entities = ctypes.c_uint()
        events = ctypes.c_uint()
        paths = ctypes.c_uint()
        if self.library.ml_bridge_counts(
            ctypes.byref(entities), ctypes.byref(events), ctypes.byref(paths)
        ):
            raise MakerLiteRuntimeError("shared core count query failed")
        return entities.value, events.value, paths.value

    def entities(self) -> list[dict]:
        entity_count, _events, _paths = self.counts()
        result = []
        for index in range(entity_count):
            x = ctypes.c_int()
            y = ctypes.c_int()
            alive = ctypes.c_int()
            kind = ctypes.c_int()
            render_cell = ctypes.c_int()
            if self.library.ml_bridge_entity(
                index,
                ctypes.byref(x),
                ctypes.byref(y),
                ctypes.byref(alive),
                ctypes.byref(kind),
                ctypes.byref(render_cell),
            ):
                raise MakerLiteRuntimeError("shared core entity query failed")
            result.append(
                {
                    "x": x.value,
                    "y": y.value,
                    "alive": bool(alive.value),
                    "kind": kind.value,
                    "render_cell": render_cell.value,
                }
            )
        return result

    def dynamics(self) -> list[dict]:
        result = []
        count = int(self.library.ml_bridge_dynamic_count())
        for index in range(count):
            x = ctypes.c_int()
            y = ctypes.c_int()
            kind = ctypes.c_int()
            if self.library.ml_bridge_dynamic(
                index, ctypes.byref(x), ctypes.byref(y), ctypes.byref(kind)
            ):
                raise MakerLiteRuntimeError("shared core dynamic query failed")
            result.append({"x": x.value, "y": y.value, "kind": kind.value})
        return result

    def event_fired(self, index: int) -> bool:
        result = self.library.ml_bridge_event_fired(ctypes.c_uint(index))
        if result < 0:
            raise MakerLiteRuntimeError("shared core event query failed")
        return bool(result)

    def pending_effect(self) -> int:
        return int(self.library.ml_bridge_pending_effect())

    def digest(self) -> int:
        """Return the canonical deterministic digest for the complete world."""

        return int(self.library.ml_bridge_digest())


class MakerLiteRuntime:
    def __init__(self, repo_root: str, private_root: str):
        self.repo_root = os.path.abspath(repo_root)
        self.cache_root = os.path.join(os.path.abspath(private_root), "runtime")
        self.executable = os.path.join(self.cache_root, "maker_lite_host_gate")
        self.shared_library = os.path.join(
            self.cache_root, "libmaker_lite_preview.so"
        )

    def _sources(self) -> list[str]:
        return [
            os.path.join(self.repo_root, "lib", "maker_lite", "maker_lite.c"),
            os.path.join(self.repo_root, "tools", "maker_lite_host_gate.c"),
        ]

    def _shared_sources(self) -> list[str]:
        return [
            os.path.join(self.repo_root, "lib", "maker_lite", "maker_lite.c"),
            os.path.join(self.repo_root, "tools", "maker_lite_bridge.c"),
        ]

    def ensure_built(self) -> str:
        os.makedirs(self.cache_root, exist_ok=True)
        sources = self._sources()
        if os.path.isfile(self.executable):
            executable_mtime = os.path.getmtime(self.executable)
            if all(os.path.getmtime(path) <= executable_mtime for path in sources):
                return self.executable
        command = [
            os.environ.get("CC", "cc"),
            "-std=c99",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            os.path.join(self.repo_root, "lib", "maker_lite"),
            *sources,
            "-o",
            self.executable,
        ]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode:
            raise MakerLiteRuntimeError(result.stderr.strip() or "C runtime build failed")
        return self.executable

    def ensure_shared(self) -> str:
        os.makedirs(self.cache_root, exist_ok=True)
        sources = self._shared_sources()
        if os.path.isfile(self.shared_library):
            library_mtime = os.path.getmtime(self.shared_library)
            if all(os.path.getmtime(path) <= library_mtime for path in sources):
                return self.shared_library
        command = [
            os.environ.get("CC", "cc"),
            "-std=c99",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fPIC",
            "-shared",
            "-I",
            os.path.join(self.repo_root, "lib", "maker_lite"),
            *sources,
            "-o",
            self.shared_library,
        ]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode:
            raise MakerLiteRuntimeError(
                result.stderr.strip() or "C preview runtime build failed"
            )
        return self.shared_library

    def open_session(self, source: dict) -> MakerLiteSession:
        library = ctypes.CDLL(self.ensure_shared())
        library.ml_bridge_open.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        library.ml_bridge_open.restype = ctypes.c_int
        library.ml_bridge_reset.argtypes = []
        library.ml_bridge_reset.restype = ctypes.c_int
        library.ml_bridge_tick.argtypes = [ctypes.c_uint32]
        library.ml_bridge_tick.restype = ctypes.c_int
        library.ml_bridge_snapshot.argtypes = [ctypes.POINTER(_Snapshot)]
        library.ml_bridge_snapshot.restype = ctypes.c_int
        library.ml_bridge_entity.argtypes = [
            ctypes.c_uint,
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
        ]
        library.ml_bridge_entity.restype = ctypes.c_int
        library.ml_bridge_counts.argtypes = [
            ctypes.POINTER(ctypes.c_uint),
            ctypes.POINTER(ctypes.c_uint),
            ctypes.POINTER(ctypes.c_uint),
        ]
        library.ml_bridge_counts.restype = ctypes.c_int
        library.ml_bridge_event_fired.argtypes = [ctypes.c_uint]
        library.ml_bridge_event_fired.restype = ctypes.c_int
        library.ml_bridge_pending_effect.argtypes = []
        library.ml_bridge_pending_effect.restype = ctypes.c_int
        library.ml_bridge_dynamic_count.argtypes = []
        library.ml_bridge_dynamic_count.restype = ctypes.c_uint
        library.ml_bridge_dynamic.argtypes = [
            ctypes.c_uint,
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
        ]
        library.ml_bridge_dynamic.restype = ctypes.c_int
        library.ml_bridge_digest.argtypes = []
        library.ml_bridge_digest.restype = ctypes.c_uint32
        library.ml_bridge_snapshot_size.argtypes = []
        library.ml_bridge_snapshot_size.restype = ctypes.c_size_t
        if library.ml_bridge_snapshot_size() != ctypes.sizeof(_Snapshot):
            raise MakerLiteRuntimeError("shared core snapshot ABI mismatch")
        return MakerLiteSession(library, compile_project(source))

    def test_project(self, source: dict, ticks: int = 600) -> str:
        executable = self.ensure_built()
        with tempfile.NamedTemporaryFile(suffix=".mlp") as pack:
            pack.write(compile_project(source))
            pack.flush()
            result = subprocess.run(
                [executable, pack.name, str(max(1, min(int(ticks), 36000)))],
                capture_output=True,
                text=True,
                check=False,
            )
        if result.returncode:
            raise MakerLiteRuntimeError(result.stderr.strip() or "Runtime test failed")
        return result.stdout.strip()
