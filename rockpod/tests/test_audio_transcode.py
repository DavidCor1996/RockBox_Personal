from services.audio_transcode import AudioSyncTranscoder


def test_temporary_output_path_preserves_audio_extension():
    path = "/tmp/device_transcodes/track-name.mp3"

    assert AudioSyncTranscoder._temporary_output_path(path) == "/tmp/device_transcodes/track-name.tmp.mp3"


def test_temporary_output_path_falls_back_without_extension():
    path = "/tmp/device_transcodes/track-name"

    assert AudioSyncTranscoder._temporary_output_path(path) == "/tmp/device_transcodes/track-name.tmp"
