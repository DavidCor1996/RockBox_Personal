from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_aac_chunk_offsets_keep_unsigned_stco_range():
    header = (
        ROOT / "lib/rbcodec/codecs/libm4a/m4a.h"
    ).read_text(encoding="utf-8")
    implementation = (
        ROOT / "lib/rbcodec/codecs/libm4a/m4a.c"
    ).read_text(encoding="utf-8")
    codec = (ROOT / "lib/rbcodec/codecs/aac.c").read_text(
        encoding="utf-8"
    )

    assert "int64_t m4a_check_sample_offset" in header
    assert "int64_t m4a_check_sample_offset" in implementation
    assert "return (int64_t)demux_res->lookup_table[i].offset;" in implementation
    assert "int64_t file_offset;" in codec
    assert "int file_offset;" not in codec


def test_audio_simulator_can_seek_into_large_h264_files():
    simulator = (ROOT / "apps/video_playback_sim.c").read_text(
        encoding="utf-8"
    )
    gate = (ROOT / "tools/h264_audio_sim_gate.py").read_text(
        encoding="utf-8"
    )

    assert "ROCKPOD_SIM_H264_AUDIO_START_MS" in simulator
    assert "video_audio_seek(test_start_ms);" in simulator
    assert '"twitch-app:"' in simulator
    assert '!strncmp(parameter, "twitch-live:", 12)' in simulator
    assert '"--start-ms"' in gate
