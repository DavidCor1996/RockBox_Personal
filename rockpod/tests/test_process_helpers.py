from types import SimpleNamespace

from ui import process_helpers


class _FakeSignal:
    def __init__(self):
        self.connected = []

    def connect(self, callback):
        self.connected.append(callback)


class _FakeProcess:
    MergedChannels = object()
    NormalExit = object()

    def __init__(self, parent=None):
        self.parent = parent
        self.readyReadStandardOutput = _FakeSignal()
        self.readyReadStandardError = _FakeSignal()
        self.finished = _FakeSignal()
        self.errorOccurred = _FakeSignal()
        self.environment = None
        self.working_directory = ""
        self.started = None
        self.program = ""
        self.arguments = []
        self.channel_mode = None
        self.output = b"line one\nline two\n"
        self.error_output = b""
        self.exit_code = 0
        self.exit_status = self.NormalExit
        self.error_string = ""

    def setProcessEnvironment(self, env):
        self.environment = env

    def processEnvironment(self):
        return self.environment or process_helpers.QProcessEnvironment.systemEnvironment()

    def setWorkingDirectory(self, path):
        self.working_directory = path

    def setProcessChannelMode(self, mode):
        self.channel_mode = mode

    def setProgram(self, program):
        self.program = program

    def setArguments(self, arguments):
        self.arguments = list(arguments)

    def start(self, program=None, args=None):
        if program is not None:
            self.started = (program, list(args or []))
        else:
            self.started = (self.program, list(self.arguments))

    def waitForStarted(self, _timeout):
        return True

    def readAllStandardOutput(self):
        data = self.output
        self.output = b""
        return data

    def readAllStandardError(self):
        data = self.error_output
        self.error_output = b""
        return data

    def exitCode(self):
        return self.exit_code

    def exitStatus(self):
        return self.exit_status

    def errorString(self):
        return self.error_string


class _FakeEventLoop:
    def __init__(self, parent=None):
        self.parent = parent
        self.executed = False

    def exec(self):
        self.executed = True

    def quit(self):
        pass


def test_create_child_process_configures_qprocess(monkeypatch):
    monkeypatch.setattr(process_helpers, "QProcess", _FakeProcess)
    request = SimpleNamespace(command=["tool"], env={"ROCKPOD": "1"})

    process = process_helpers.create_child_process(
        object(),
        request,
        "/tmp/work",
        lambda: None,
        lambda: None,
        lambda: None,
    )

    assert process.working_directory == "/tmp/work"
    assert process.environment.value("ROCKPOD") == "1"
    assert len(process.readyReadStandardOutput.connected) == 1
    assert len(process.readyReadStandardError.connected) == 1
    assert len(process.finished.connected) == 1
    assert len(process.errorOccurred.connected) == 1


def test_run_blocking_merged_process_collects_output(monkeypatch):
    monkeypatch.setattr(process_helpers, "QProcess", _FakeProcess)
    monkeypatch.setattr(process_helpers, "QEventLoop", _FakeEventLoop)
    seen = []

    result = process_helpers.run_blocking_merged_process(
        object(),
        ["tool", "--run"],
        on_output=seen.append,
    )

    assert result.returncode == 0
    assert result.stdout == "line one\nline two\n"
    assert result.stderr == ""
    assert seen == ["line one\nline two\n"]


def test_start_qprocess_sets_command_environment_and_handlers(monkeypatch):
    monkeypatch.setattr(process_helpers, "QProcess", _FakeProcess)

    process = process_helpers.start_qprocess(
        object(),
        ["sim", "--root", "/tmp/simdisk"],
        cwd="/tmp/build",
        env={"RBROOT": "/tmp"},
        error_handler=lambda _error: None,
        finished_handler=lambda _code, _status: None,
    )

    assert process.working_directory == "/tmp/build"
    assert process.started == ("sim", ["--root", "/tmp/simdisk"])
    assert process.environment.value("RBROOT") == "/tmp"
    assert len(process.errorOccurred.connected) == 1
    assert len(process.finished.connected) == 1


def test_start_hidden_process_merges_environment(monkeypatch):
    launched = {}

    class _Popen:
        def __init__(self, command, **kwargs):
            launched["command"] = list(command)
            launched.update(kwargs)

    monkeypatch.setattr(process_helpers.subprocess, "Popen", _Popen)
    monkeypatch.setenv("PATH", "/usr/bin")

    process = process_helpers.start_hidden_process(
        ["sim", "--root", "/tmp/simdisk"],
        cwd="/tmp/build",
        env={"ROCKPOD_SIM_HIDDEN": "1"},
    )

    assert isinstance(process, _Popen)
    assert launched["command"] == ["sim", "--root", "/tmp/simdisk"]
    assert launched["cwd"] == "/tmp/build"
    assert launched["env"]["PATH"] == "/usr/bin"
    assert launched["env"]["ROCKPOD_SIM_HIDDEN"] == "1"
    assert launched["start_new_session"] is True


def test_start_detached_command_suppresses_output(monkeypatch):
    launched = {}

    class _Popen:
        def __init__(self, command, **kwargs):
            launched["command"] = list(command)
            launched.update(kwargs)

    monkeypatch.setattr(process_helpers.subprocess, "Popen", _Popen)

    process = process_helpers.start_detached_command(["xdg-open", "/tmp/screens"])

    assert isinstance(process, _Popen)
    assert launched["command"] == ["xdg-open", "/tmp/screens"]
    assert launched["stdout"] is process_helpers.subprocess.DEVNULL
    assert launched["stderr"] is process_helpers.subprocess.DEVNULL
    assert launched["start_new_session"] is True


def test_process_output_helpers_read_compact_and_split():
    process = _FakeProcess()
    process.output = " first line \n\n second line\n".encode("utf-8")
    process.error_output = " warning line \n".encode("utf-8")

    raw = process_helpers.read_process_text(process)

    assert raw == " first line \n\n second line\n warning line \n"
    assert process_helpers.process_output_lines(raw) == ["first line", "second line", "warning line"]
    assert process_helpers.compact_process_text(raw) == "first line second line warning line"
    assert process_helpers.read_process_text(None) == ""
