"""Shared command execution helpers for RockPod service workflows."""

from __future__ import annotations

import os
import subprocess
from dataclasses import dataclass
from datetime import datetime


@dataclass(frozen=True)
class CommandResult:
    command: list[str]
    cwd: str
    returncode: int
    stdout: str
    stderr: str
    log_path: str

    @property
    def success(self):
        return self.returncode == 0

    @property
    def display_command(self):
        return " ".join(self.command)

    def failure_message(self):
        if self.success:
            return ""
        details = f"Command failed with exit status {self.returncode}: {self.display_command}"
        if self.log_path:
            details = f"{details}\nLog: {self.log_path}"
        return details


class CommandRunner:
    """Run commands with consistent output capture and optional logs."""

    def __init__(self, log_dir=""):
        self.log_dir = os.path.abspath(log_dir) if log_dir else ""

    def run(self, command, cwd="", timeout=None, env=None):
        cmd = [str(part) for part in command]
        workdir = os.path.abspath(cwd or os.getcwd())
        completed = subprocess.run(
            cmd,
            cwd=workdir,
            env=env,
            timeout=timeout,
            check=False,
            capture_output=True,
            text=True,
        )
        log_path = self._write_log(cmd, workdir, completed.returncode, completed.stdout, completed.stderr)
        return CommandResult(
            command=cmd,
            cwd=workdir,
            returncode=completed.returncode,
            stdout=completed.stdout,
            stderr=completed.stderr,
            log_path=log_path,
        )

    def _write_log(self, command, cwd, returncode, stdout, stderr):
        if not self.log_dir:
            return ""
        os.makedirs(self.log_dir, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        log_path = os.path.join(self.log_dir, f"rockpod-command-{stamp}.log")
        with open(log_path, "w", encoding="utf-8") as handle:
            handle.write(f"Command: {' '.join(command)}\n")
            handle.write(f"Cwd: {cwd}\n")
            handle.write(f"Exit status: {returncode}\n")
            handle.write("\n[stdout]\n")
            handle.write(stdout or "")
            handle.write("\n[stderr]\n")
            handle.write(stderr or "")
        return log_path
