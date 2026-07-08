"""Qt process helpers shared by RockPod UI workflows."""

from __future__ import annotations

import os
import subprocess
from dataclasses import dataclass

from PySide6.QtCore import QEventLoop, QProcess, QProcessEnvironment


@dataclass
class BlockingProcessResult:
    returncode: int
    stdout: str
    stderr: str


def create_child_process(parent, request, working_directory, output_handler, finished_handler, error_handler):
    process = QProcess(parent)
    env = QProcessEnvironment.systemEnvironment()
    for key, value in getattr(request, "env", {}).items():
        env.insert(str(key), str(value))
    process.setProcessEnvironment(env)
    process.setWorkingDirectory(working_directory)
    process.readyReadStandardOutput.connect(output_handler)
    process.readyReadStandardError.connect(output_handler)
    process.finished.connect(finished_handler)
    process.errorOccurred.connect(error_handler)
    return process


def run_blocking_merged_process(parent, command, on_output=None, start_timeout_ms=3000):
    process = QProcess(parent)
    process.setProcessChannelMode(QProcess.MergedChannels)
    output_parts = []

    def read_output():
        data = bytes(process.readAllStandardOutput()).decode("utf-8", "replace")
        if not data:
            return
        output_parts.append(data)
        if on_output is not None:
            on_output(data)

    loop = QEventLoop(parent)
    process.readyReadStandardOutput.connect(read_output)
    process.finished.connect(lambda _code, _status: loop.quit())
    process.errorOccurred.connect(lambda _error: loop.quit())
    process.start(command[0], command[1:])
    if not process.waitForStarted(start_timeout_ms):
        return BlockingProcessResult(
            returncode=127,
            stdout="".join(output_parts),
            stderr=process.errorString(),
        )
    loop.exec()
    read_output()
    return BlockingProcessResult(
        returncode=process.exitCode(),
        stdout="".join(output_parts),
        stderr=process.errorString() if process.exitStatus() != QProcess.NormalExit else "",
    )


def start_qprocess(parent, command, cwd="", env=None, error_handler=None, finished_handler=None):
    process = QProcess(parent)
    if cwd:
        process.setWorkingDirectory(cwd)
    process_env = process.processEnvironment()
    for key, value in (env or {}).items():
        process_env.insert(str(key), str(value))
    process.setProcessEnvironment(process_env)
    process.setProgram(command[0])
    process.setArguments(list(command[1:]))
    if error_handler is not None:
        process.errorOccurred.connect(error_handler)
    if finished_handler is not None:
        process.finished.connect(finished_handler)
    process.start()
    return process


def start_hidden_process(command, cwd="", env=None):
    process_env = os.environ.copy()
    process_env.update({str(key): str(value) for key, value in (env or {}).items()})
    return subprocess.Popen(
        command,
        cwd=cwd or None,
        env=process_env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )


def start_detached_command(command):
    return subprocess.Popen(
        command,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )


def read_process_text(process):
    if process is None:
        return ""
    stdout = bytes(process.readAllStandardOutput()).decode("utf-8", "replace")
    stderr = bytes(process.readAllStandardError()).decode("utf-8", "replace")
    return stdout + stderr


def process_output_lines(text):
    return [line.strip() for line in str(text or "").splitlines() if line.strip()]


def compact_process_text(text):
    return " ".join(process_output_lines(text))
