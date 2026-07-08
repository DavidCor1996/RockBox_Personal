import os
import sys

from services.command_runner import CommandRunner


def test_command_runner_captures_success_and_log(tmp_dir):
    runner = CommandRunner(log_dir=os.path.join(tmp_dir, "logs"))

    result = runner.run([sys.executable, "-c", "print('ready')"], cwd=tmp_dir)

    assert result.success is True
    assert result.returncode == 0
    assert result.stdout.strip() == "ready"
    assert result.stderr == ""
    assert result.cwd == os.path.abspath(tmp_dir)
    assert os.path.isfile(result.log_path)
    with open(result.log_path, "r", encoding="utf-8") as handle:
        log = handle.read()
    assert "Exit status: 0" in log
    assert "ready" in log


def test_command_runner_failure_message_includes_exit_status_and_log(tmp_dir):
    runner = CommandRunner(log_dir=os.path.join(tmp_dir, "logs"))

    result = runner.run(
        [
            sys.executable,
            "-c",
            "import sys; print('bad stdout'); print('bad stderr', file=sys.stderr); sys.exit(7)",
        ],
        cwd=tmp_dir,
    )

    assert result.success is False
    assert result.returncode == 7
    assert "bad stdout" in result.stdout
    assert "bad stderr" in result.stderr
    assert "exit status 7" in result.failure_message()
    assert result.log_path in result.failure_message()
