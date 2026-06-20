from ui.store_workflows import (
    store_import_empty_message,
    store_import_failure_message,
    store_import_success_message,
)


def test_store_import_failure_message_uses_recent_output_and_log_path():
    message = store_import_failure_message(2, ["first", "last line"], "/tmp/import.log")

    assert message == "streamrip failed with exit code 2: first last line\nLog: /tmp/import.log"


def test_store_import_empty_message_falls_back_to_auth_hint():
    message = store_import_empty_message([], "/tmp/import.log")

    assert "No new audio files detected" in message
    assert "Check Tidal/Qobuz auth" in message
    assert message.endswith("Log: /tmp/import.log")


def test_store_import_success_message_pluralizes_counts():
    one = store_import_success_message(1, 1, "/tmp/one.log")
    many = store_import_success_message(3, 2, "/tmp/many.log")

    assert "Imported 1 audio file; added 1 Rockbox cover file." in one
    assert "Imported 3 audio files; added 2 Rockbox cover files." in many
