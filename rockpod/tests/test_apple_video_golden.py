import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.apple_video_golden import (  # noqa: E402
    QUICKTIME_H264_SHA256,
    SCHEMA,
    compare_movie_contract,
    validate_ipod_output,
    verify_manifest,
)
from services.apple_video_exact import movie_header_timescale  # noqa: E402


def _atom(kind: bytes, payload: bytes) -> bytes:
    return (len(payload) + 8).to_bytes(4, "big") + kind + payload


def test_movie_header_timescale_reads_movie_edit_clock(tmp_path):
    movie = tmp_path / "clock.m4v"
    mvhd = _atom(
        b"mvhd",
        b"\x00\x00\x00\x00" + b"\x00" * 8 + (1000).to_bytes(4, "big"),
    )
    movie.write_bytes(_atom(b"moov", mvhd))

    assert movie_header_timescale(movie) == 1000


def _valid_movie():
    return {
        "path": "case.m4v",
        "streams": [
            {
                "index": 0,
                "codec_type": "video",
                "codec_name": "h264",
                "profile": "Constrained Baseline",
                "level": 30,
                "width": 640,
                "height": 480,
                "r_frame_rate": "30000/1001",
                "has_b_frames": 0,
                "refs": 1,
                "pix_fmt": "yuv420p",
                "tags": {"handler_name": "Apple Video Media Handler"},
            },
            {
                "index": 1,
                "codec_type": "audio",
                "codec_name": "aac",
                "profile": "LC",
                "sample_rate": "44100",
                "channels": 2,
                "tags": {"handler_name": "Apple Sound Media Handler"},
            },
        ],
        "atoms": [
            {"type": "ftyp", "offset": 0, "size": 20},
            {"type": "moov", "offset": 20, "size": 8},
            {"type": "mdat", "offset": 28, "size": 9},
        ],
        "avc_contract": {
            "sps": {
                "nal_ref_idc": 1,
                "profile_idc": 66,
                "constraint_flags": 224,
                "level_idc": 30,
                "log2_max_frame_num_minus4": 1,
                "pic_order_cnt_type": 0,
                "log2_max_pic_order_cnt_lsb_minus4": 3,
                "max_num_ref_frames": 1,
                "coded_width": 640,
                "coded_height": 480,
                "width": 640,
                "height": 480,
                "vui_parameters_present": 1,
                "aspect_ratio_info_present": 1,
                "aspect_ratio_idc": 0,
                "overscan_info_present": 0,
                "video_signal_type_present": 1,
                "video_format": 5,
                "video_full_range": 0,
                "colour_description_present": 1,
                "colour_primaries": 6,
                "transfer_characteristics": 1,
                "matrix_coefficients": 6,
                "chroma_loc_info_present": 1,
                "chroma_sample_loc_type_top_field": 2,
                "chroma_sample_loc_type_bottom_field": 2,
                "timing_info_present": 0,
                "nal_hrd_parameters_present": 1,
                "nal_hrd": {
                    "cpb_count": 1,
                    "bit_rate_scale": 7,
                    "cpb_size_scale": 10,
                    "bit_rate_value_minus1": [488],
                    "cpb_size_value_minus1": [244],
                    "cbr_flag": [0],
                    "initial_cpb_removal_delay_length_minus1": 23,
                    "cpb_removal_delay_length_minus1": 23,
                    "dpb_output_delay_length_minus1": 23,
                    "time_offset_length": 24,
                },
                "vcl_hrd_parameters_present": 0,
                "low_delay_hrd": 0,
                "pic_struct_present": 0,
                "bitstream_restriction": 0,
            },
            "pps": {
                "nal_ref_idc": 1,
                "pps_id": 0,
                "sps_id": 0,
                "entropy_coding_mode": 0,
                "bottom_field_pic_order_in_frame_present": 1,
                "num_ref_idx_l0_default_active_minus1": 0,
                "num_ref_idx_l1_default_active_minus1": 0,
                "weighted_pred": 0,
                "weighted_bipred_idc": 0,
                "pic_init_qp_minus26": 2,
                "pic_init_qs_minus26": 0,
                "chroma_qp_index_offset": 0,
                "deblocking_filter_control_present": 1,
                "constrained_intra_pred": 0,
                "redundant_pic_cnt_present": 0,
            },
            "length_size": 4,
            "nal_ref_idc": {
                "1": [0, 1], "5": [1], "7": [1], "8": [1],
            },
            "pictures": 1,
            "slices_per_picture": [2],
            "nal_types_per_picture": [[6, 1, 1], [6, 6, 5, 5]],
            "sei_nals": [
                "0600078493e0000003004080",
                "0605110387f44ecd0a4bdca1943ac3d49b171f0180",
            ],
            "first_mb_in_slice": [[0, 600]],
            "disable_deblocking_filter_idc": [1],
        },
    }


def test_apple_golden_validator_accepts_narrow_ipod_contract():
    assert validate_ipod_output(_valid_movie()) == []


def test_apple_golden_validator_rejects_unsupported_h264_tools():
    movie = _valid_movie()
    video = movie["streams"][0]
    video.update(
        {
            "profile": "Main",
            "level": 31,
            "width": 1280,
            "r_frame_rate": "60/1",
            "has_b_frames": 2,
            "refs": 4,
        }
    )

    failures = validate_ipod_output(movie)

    assert "video is not H.264 Baseline" in failures
    assert "H.264 level is not <= 3.0" in failures
    assert "coded dimensions exceed 640x480" in failures
    assert "frame rate exceeds 30 fps" in failures
    assert "B-frames are present" in failures
    assert "more than one reference picture is present" in failures


def test_candidate_comparison_requires_observed_apple_structure():
    apple = _valid_movie()
    apple.update(
        {
            "size": 1000,
            "ftyp": {
                "major_brand": "M4V ",
                "minor_version": 1,
                "compatible_brands": ["M4V ", "mp42", "isom"],
            },
        }
    )
    apple["streams"][0].update(
        {
            "time_base": "1/30000",
            "avg_frame_rate": "30000/1001",
            "sample_aspect_ratio": "1:1",
            "bit_rate": "1500000",
        }
    )
    apple["streams"][1].update(
        {"time_base": "1/44100", "channel_layout": "stereo"}
    )
    candidate = json.loads(json.dumps(apple))
    candidate["size"] = 900
    candidate["streams"][0]["bit_rate"] = "1350000"

    failures, measurements = compare_movie_contract(apple, candidate)

    assert failures == []
    assert measurements["candidate_to_apple_rate"] == 0.9

    candidate["streams"][0]["width"] = 320
    candidate["ftyp"]["major_brand"] = "isom"
    failures, _measurements = compare_movie_contract(apple, candidate)
    assert any("video width differs" in item for item in failures)
    assert "ftyp brand contract differs" in failures


def test_unqualified_manifest_cannot_open_exact_apple_gate(tmp_path):
    source_dir = tmp_path / "sources"
    output_dir = tmp_path / "outputs"
    source_dir.mkdir()
    output_dir.mkdir()
    manifest = tmp_path / "golden.json"
    manifest.write_text(
        json.dumps(
            {
                "schema": SCHEMA,
                "kind": "apple-output-contract",
                "qualified": False,
                "reserved_sync_test_downloaded": True,
                "provenance": {
                    "itunes_installer_sha1": "invalid",
                    "quicktime_h264_sha256": QUICKTIME_H264_SHA256,
                    "operator_attestation": "",
                },
                "cases": [],
            }
        ),
        encoding="utf-8",
    )

    failures = verify_manifest(manifest, source_dir, output_dir)

    assert "manifest is not qualified" in failures
    assert "reserved YouTube test must remain untouched before qualification" in failures
    assert "iTunes installer provenance is invalid" in failures
    assert "Apple menu-action attestation is missing" in failures
    assert "golden corpus case set is incomplete" in failures
