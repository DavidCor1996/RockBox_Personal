import os

from services.audio_transcode import AudioSyncTranscoder


class _FakeCommandResult:
    def __init__(self, returncode=0, stdout="", stderr="", log_path=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.log_path = log_path

    def failure_message(self):
        return f"Command failed with exit status {self.returncode}: fake ffmpeg\nLog: {self.log_path}"


class _FakeCommandRunner:
    def __init__(self, result=None, write_output=True):
        self.result = result or _FakeCommandResult()
        self.write_output = write_output
        self.commands = []
        self.cwd = ""

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append(command)
        self.cwd = cwd
        if self.write_output and self.result.returncode == 0:
            with open(command[-1], "wb") as handle:
                handle.write(b"converted")
        return self.result


def test_temporary_output_path_preserves_audio_extension():
    path = "/tmp/device_transcodes/track-name.mp3"

    assert AudioSyncTranscoder._temporary_output_path(path) == "/tmp/device_transcodes/track-name.tmp.mp3"


def test_temporary_output_path_falls_back_without_extension():
    path = "/tmp/device_transcodes/track-name"

    assert AudioSyncTranscoder._temporary_output_path(path) == "/tmp/device_transcodes/track-name.tmp"


def test_ensure_transcode_uses_shared_command_runner(tmp_dir):
    source = os.path.join(tmp_dir, "source.flac")
    cache = os.path.join(tmp_dir, "cache", "track.mp3")
    with open(source, "wb") as handle:
        handle.write(b"source")
    runner = _FakeCommandRunner()
    transcoder = AudioSyncTranscoder(os.path.join(tmp_dir, "cache"), command_runner=runner)

    transcoder._ensure_transcode("ffmpeg", source, cache, "mp3", 160)

    assert os.path.isfile(cache)
    assert runner.cwd == os.path.dirname(cache)
    assert runner.commands[0][-1] == os.path.join(tmp_dir, "cache", "track.tmp.mp3")
    assert "-c:a" in runner.commands[0]


def test_ensure_transcode_reports_runner_failure_and_removes_temp(tmp_dir):
    source = os.path.join(tmp_dir, "source.flac")
    cache = os.path.join(tmp_dir, "cache", "track.mp3")
    with open(source, "wb") as handle:
        handle.write(b"source")
    result = _FakeCommandResult(returncode=7, stderr="encoder failed", log_path=os.path.join(tmp_dir, "ffmpeg.log"))
    runner = _FakeCommandRunner(result=result, write_output=False)
    transcoder = AudioSyncTranscoder(os.path.join(tmp_dir, "cache"), command_runner=runner)

    try:
        transcoder._ensure_transcode("ffmpeg", source, cache, "mp3", 160)
    except RuntimeError as exc:
        assert "audio conversion failed: encoder failed" in str(exc)
    else:
        raise AssertionError("Transcode failure was not reported")

    assert not os.path.exists(cache)
    assert not os.path.exists(os.path.join(tmp_dir, "cache", "track.tmp.mp3"))
