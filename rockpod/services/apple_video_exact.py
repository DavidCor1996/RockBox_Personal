"""Measured iTunes 9.2.1 iPod-video container and H.264 contract helpers."""

from __future__ import annotations

import os
import re
import shutil
from pathlib import Path


APPLE_FTYP = (
    b"\x00\x00\x00\x20ftyp"
    b"M4V \x00\x00\x00\x01"
    b"M4V M4A mp42isom"
)
APPLE_TOP_LEVEL_ATOMS = ("ftyp", "moov", "free", "free", "mdat")
APPLE_X264_MARKER = "RockPod Apple-iPod exact bitstream patch v5"
APPLE_X264_NAME = "x264-apple-ipod"

_CONTAINER_ATOMS = {
    b"moov", b"trak", b"mdia", b"minf", b"stbl", b"edts", b"dinf",
    b"udta", b"ilst", b"tref", b"ipro", b"sinf", b"schi",
}


class AppleVideoContractError(RuntimeError):
    """An output does not match the measured Apple iPod-video contract."""


def local_x264_path() -> str:
    """Return RockPod's conventional local path for the patched encoder."""
    return str(Path(__file__).resolve().parents[1] / ".tools" / APPLE_X264_NAME)


def apple_video_timescale(fps_num: int, fps_den: int) -> int:
    """Return the video media timescale observed in iTunes 9.2.1 outputs."""
    if fps_num <= 0 or fps_den <= 0:
        raise AppleVideoContractError("invalid video frame rate")
    if fps_den == 1:
        return fps_num * 512
    return fps_num


def apple_h264_level(width: int, height: int) -> tuple[str, int]:
    """Select Apple's measured small-picture or SD H.264 level bucket."""
    if width <= 0 or height <= 0 or width > 640 or height > 480:
        raise AppleVideoContractError("iPod video dimensions exceed 640x480")
    return ("1.3", 13) if width * height <= 320 * 240 else ("3.0", 30)


def _atom_extent(data: bytes | bytearray, offset: int, end: int) -> tuple[bytes, int, int]:
    if offset + 8 > end:
        raise AppleVideoContractError("truncated MP4 atom header")
    size = int.from_bytes(data[offset:offset + 4], "big")
    atom_type = bytes(data[offset + 4:offset + 8])
    header_size = 8
    if size == 1:
        if offset + 16 > end:
            raise AppleVideoContractError("truncated 64-bit MP4 atom header")
        size = int.from_bytes(data[offset + 8:offset + 16], "big")
        header_size = 16
    elif size == 0:
        size = end - offset
    if size < header_size or offset + size > end:
        raise AppleVideoContractError("invalid MP4 atom extent")
    return atom_type, size, header_size


def top_level_atoms(path: str | os.PathLike[str]) -> list[tuple[str, int, int]]:
    atoms = []
    file_size = os.path.getsize(path)
    with open(path, "rb") as stream:
        offset = 0
        while offset < file_size:
            stream.seek(offset)
            head = stream.read(16)
            if len(head) < 8:
                raise AppleVideoContractError("truncated MP4 atom header")
            size = int.from_bytes(head[:4], "big")
            header = 8
            if size == 1:
                if len(head) < 16:
                    raise AppleVideoContractError("truncated extended MP4 atom")
                size = int.from_bytes(head[8:16], "big"); header = 16
            elif size == 0:
                size = file_size - offset
            if size < header or size > file_size - offset:
                raise AppleVideoContractError("invalid MP4 atom extent")
            atoms.append((head[4:8].decode("latin-1"), offset, size))
            if len(atoms) > 4096:
                raise AppleVideoContractError("too many top-level atoms")
            offset += size
    return atoms


def _read_moov(path):
    atoms = [a for a in top_level_atoms(path) if a[0] == "moov"]
    if len(atoms) != 1 or atoms[0][2] > 64 * 1024 * 1024:
        raise AppleVideoContractError("missing/duplicate/oversized movie index")
    with open(path, "rb") as stream:
        stream.seek(atoms[0][1])
        return stream.read(atoms[0][2])


def movie_header_timescale(path: str | os.PathLike[str]) -> int:
    """Return the MP4 ``mvhd`` clock for diagnostics and golden comparison.

    This is the movie-edit clock, not the timestamp unit passed by RetailOS
    to the VideoCore MPlayer service.  Apple's per-track readers normalize
    AAC and H.264 media timestamps to milliseconds before that IPC boundary.
    """
    data = _read_moov(path)
    offset = 0
    while offset < len(data):
        atom_type, size, header_size = _atom_extent(data, offset, len(data))
        if atom_type == b"moov":
            child = offset + header_size
            end = offset + size
            while child < end:
                child_type, child_size, child_header = _atom_extent(
                    data, child, end
                )
                if child_type == b"mvhd":
                    payload = child + child_header
                    if payload + 4 > child + child_size:
                        raise AppleVideoContractError("truncated MP4 movie header")
                    version = data[payload]
                    timescale_offset = payload + (20 if version == 1 else 12)
                    if version not in {0, 1} or timescale_offset + 4 > child + child_size:
                        raise AppleVideoContractError("invalid MP4 movie header")
                    return int.from_bytes(
                        data[timescale_offset:timescale_offset + 4], "big"
                    )
                child += child_size
            raise AppleVideoContractError("MP4 has no movie header")
        offset += size
    raise AppleVideoContractError("MP4 has no moov atom")


def _patch_chunk_offsets(data: bytearray, start: int, end: int, delta: int) -> int:
    """Patch stco/co64 absolute media offsets inside a normal moov hierarchy."""
    patched = 0
    offset = start
    while offset < end:
        atom_type, size, header_size = _atom_extent(data, offset, end)
        payload = offset + header_size
        atom_end = offset + size
        if atom_type in {b"stco", b"co64"}:
            if payload + 8 > atom_end:
                raise AppleVideoContractError("truncated MP4 chunk-offset table")
            count = int.from_bytes(data[payload + 4:payload + 8], "big")
            width = 4 if atom_type == b"stco" else 8
            entries = payload + 8
            if entries + count * width > atom_end:
                raise AppleVideoContractError("invalid MP4 chunk-offset table")
            for index in range(count):
                item = entries + index * width
                value = int.from_bytes(data[item:item + width], "big") + delta
                if value >= 1 << (width * 8):
                    raise AppleVideoContractError("MP4 chunk offset overflow")
                data[item:item + width] = value.to_bytes(width, "big")
                patched += 1
        elif atom_type in _CONTAINER_ATOMS:
            patched += _patch_chunk_offsets(data, payload, atom_end, delta)
        elif atom_type == b"meta":
            if payload + 4 > atom_end:
                raise AppleVideoContractError("truncated MP4 meta atom")
            patched += _patch_chunk_offsets(data, payload + 4, atom_end, delta)
        offset = atom_end
    if offset != end:
        raise AppleVideoContractError("invalid nested MP4 atom layout")
    return patched


def rewrite_mp4_as_apple(path: str | os.PathLike[str]) -> None:
    """Reproduce Apple's ftyp and two-free-atom fast-start layout in place."""
    target = Path(path)
    atoms = top_level_atoms(path)
    if [a[0] for a in atoms] != ["ftyp", "moov", "free", "mdat"]:
        raise AppleVideoContractError("unexpected pre-rewrite MP4 atom order")
    if atoms[0][2] != len(APPLE_FTYP) or atoms[2][2] != 8:
        raise AppleVideoContractError("unexpected ftyp/free sizes")
    moov = bytearray(_read_moov(path))
    _, _, header = _atom_extent(moov, 0, len(moov))
    if _patch_chunk_offsets(moov, header, len(moov), 8) <= 0:
        raise AppleVideoContractError("MP4 has no chunk offsets to relocate")
    temporary = target.with_name(target.name + ".apple-rewrite")
    try:
        with open(target, "rb") as source, open(temporary, "wb") as output:
            output.write(APPLE_FTYP)
            output.write(moov)
            output.write(b"\x00\x00\x00\x08free" * 2)
            source.seek(atoms[3][1])
            shutil.copyfileobj(source, output, 1024 * 1024)
            output.flush()
            os.fsync(output.fileno())
        if tuple(a[0] for a in top_level_atoms(temporary)) != APPLE_TOP_LEVEL_ATOMS:
            raise AppleVideoContractError("Apple MP4 rewrite did not validate")
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)


class _BitReader:
    def __init__(self, data: bytes):
        self.data = data
        self.bit = 0

    def read(self, count: int) -> int:
        if count < 0 or self.bit + count > len(self.data) * 8:
            raise AppleVideoContractError("truncated H.264 bitstream")
        value = 0
        for _ in range(count):
            value = (value << 1) | (
                (self.data[self.bit // 8] >> (7 - self.bit % 8)) & 1
            )
            self.bit += 1
        return value

    def ue(self) -> int:
        zeros = 0
        while self.read(1) == 0:
            zeros += 1
            if zeros > 31:
                raise AppleVideoContractError("invalid H.264 Exp-Golomb value")
        return (1 << zeros) - 1 + (self.read(zeros) if zeros else 0)

    def se(self) -> int:
        code = self.ue()
        return (code + 1) // 2 if code & 1 else -(code // 2)


def _rbsp(nal: bytes) -> bytes:
    output = bytearray()
    zeros = 0
    for value in nal[1:]:
        if zeros >= 2 and value == 3:
            zeros = 0
            continue
        output.append(value)
        zeros = zeros + 1 if value == 0 else 0
    return bytes(output)


def _parse_hrd(reader: _BitReader) -> dict[str, object]:
    cpb_count = reader.ue() + 1
    result: dict[str, object] = {
        "cpb_count": cpb_count,
        "bit_rate_scale": reader.read(4),
        "cpb_size_scale": reader.read(4),
    }
    bit_rate_value_minus1: list[int] = []
    cpb_size_value_minus1: list[int] = []
    cbr_flag: list[int] = []
    for _ in range(cpb_count):
        bit_rate_value_minus1.append(reader.ue())
        cpb_size_value_minus1.append(reader.ue())
        cbr_flag.append(reader.read(1))
    result["bit_rate_value_minus1"] = bit_rate_value_minus1
    result["cpb_size_value_minus1"] = cpb_size_value_minus1
    result["cbr_flag"] = cbr_flag
    result["initial_cpb_removal_delay_length_minus1"] = reader.read(5)
    result["cpb_removal_delay_length_minus1"] = reader.read(5)
    result["dpb_output_delay_length_minus1"] = reader.read(5)
    result["time_offset_length"] = reader.read(5)
    return result


def _parse_vui(reader: _BitReader) -> dict[str, object]:
    result: dict[str, object] = {}
    aspect_present = reader.read(1)
    result["aspect_ratio_info_present"] = aspect_present
    if aspect_present:
        aspect_idc = reader.read(8)
        result["aspect_ratio_idc"] = aspect_idc
        if aspect_idc == 255:
            result["sar_width"] = reader.read(16)
            result["sar_height"] = reader.read(16)
    overscan_present = reader.read(1)
    result["overscan_info_present"] = overscan_present
    if overscan_present:
        result["overscan_appropriate"] = reader.read(1)
    signal_present = reader.read(1)
    result["video_signal_type_present"] = signal_present
    if signal_present:
        result["video_format"] = reader.read(3)
        result["video_full_range"] = reader.read(1)
        colour_present = reader.read(1)
        result["colour_description_present"] = colour_present
        if colour_present:
            result["colour_primaries"] = reader.read(8)
            result["transfer_characteristics"] = reader.read(8)
            result["matrix_coefficients"] = reader.read(8)
    chroma_present = reader.read(1)
    result["chroma_loc_info_present"] = chroma_present
    if chroma_present:
        result["chroma_sample_loc_type_top_field"] = reader.ue()
        result["chroma_sample_loc_type_bottom_field"] = reader.ue()
    timing_present = reader.read(1)
    result["timing_info_present"] = timing_present
    if timing_present:
        result["num_units_in_tick"] = reader.read(32)
        result["time_scale"] = reader.read(32)
        result["fixed_frame_rate"] = reader.read(1)
    nal_hrd = reader.read(1)
    result["nal_hrd_parameters_present"] = nal_hrd
    if nal_hrd:
        result["nal_hrd"] = _parse_hrd(reader)
    vcl_hrd = reader.read(1)
    result["vcl_hrd_parameters_present"] = vcl_hrd
    if vcl_hrd:
        result["vcl_hrd"] = _parse_hrd(reader)
    if nal_hrd or vcl_hrd:
        result["low_delay_hrd"] = reader.read(1)
    result["pic_struct_present"] = reader.read(1)
    restriction = reader.read(1)
    result["bitstream_restriction"] = restriction
    if restriction:
        result["motion_vectors_over_pic_boundaries"] = reader.read(1)
        result["max_bytes_per_pic_denom"] = reader.ue()
        result["max_bits_per_mb_denom"] = reader.ue()
        result["log2_max_mv_length_horizontal"] = reader.ue()
        result["log2_max_mv_length_vertical"] = reader.ue()
        result["max_num_reorder_frames"] = reader.ue()
        result["max_dec_frame_buffering"] = reader.ue()
    return result


def parse_sps(nal: bytes) -> dict[str, int]:
    if not nal or nal[0] & 0x1F != 7:
        raise AppleVideoContractError("NAL is not an SPS")
    reader = _BitReader(_rbsp(nal))
    result = {
        "nal_ref_idc": (nal[0] >> 5) & 3,
        "profile_idc": reader.read(8),
        "constraint_flags": reader.read(8),
        "level_idc": reader.read(8),
    }
    result["sps_id"] = reader.ue()
    if result["profile_idc"] not in {66, 77, 88}:
        chroma_format_idc = reader.ue()
        if chroma_format_idc == 3:
            reader.read(1)
        reader.ue()
        reader.ue()
        reader.read(1)
        if reader.read(1):
            count = 8 if chroma_format_idc != 3 else 12
            for index in range(count):
                if reader.read(1):
                    size = 16 if index < 6 else 64
                    last = 8
                    next_scale = 8
                    for _ in range(size):
                        if next_scale:
                            next_scale = (last + reader.se() + 256) % 256
                        last = next_scale or last
    result["log2_max_frame_num_minus4"] = reader.ue()
    result["pic_order_cnt_type"] = reader.ue()
    if result["pic_order_cnt_type"] == 0:
        result["log2_max_pic_order_cnt_lsb_minus4"] = reader.ue()
    elif result["pic_order_cnt_type"] == 1:
        reader.read(1)
        reader.se()
        reader.se()
        for _ in range(reader.ue()):
            reader.se()
    result["max_num_ref_frames"] = reader.ue()
    result["gaps_allowed"] = reader.read(1)
    width_mbs = reader.ue() + 1
    height_map_units = reader.ue() + 1
    frame_mbs_only = reader.read(1)
    result["frame_mbs_only"] = frame_mbs_only
    if not frame_mbs_only:
        reader.read(1)
    reader.read(1)
    crop_left = crop_right = crop_top = crop_bottom = 0
    if reader.read(1):
        crop_left = reader.ue()
        crop_right = reader.ue()
        crop_top = reader.ue()
        crop_bottom = reader.ue()
    result["crop_left"] = crop_left
    result["crop_top"] = crop_top
    result["coded_width"] = width_mbs * 16
    result["coded_height"] = height_map_units * 16 * (2 - frame_mbs_only)
    result["width"] = result["coded_width"] - 2 * (crop_left + crop_right)
    result["height"] = result["coded_height"] - 2 * (crop_top + crop_bottom)
    vui_present = reader.read(1)
    result["vui_parameters_present"] = vui_present
    if vui_present:
        result.update(_parse_vui(reader))
    return result


def parse_pps(nal: bytes) -> dict[str, int]:
    if not nal or nal[0] & 0x1F != 8:
        raise AppleVideoContractError("NAL is not a PPS")
    reader = _BitReader(_rbsp(nal))
    result = {
        "nal_ref_idc": (nal[0] >> 5) & 3,
        "pps_id": reader.ue(),
        "sps_id": reader.ue(),
        "entropy_coding_mode": reader.read(1),
        "bottom_field_pic_order_in_frame_present": reader.read(1),
    }
    slice_groups = reader.ue()
    if slice_groups:
        raise AppleVideoContractError("slice groups are outside the Apple contract")
    result["num_ref_idx_l0_default_active_minus1"] = reader.ue()
    result["num_ref_idx_l1_default_active_minus1"] = reader.ue()
    result["weighted_pred"] = reader.read(1)
    result["weighted_bipred_idc"] = reader.read(2)
    result["pic_init_qp_minus26"] = reader.se()
    result["pic_init_qs_minus26"] = reader.se()
    result["chroma_qp_index_offset"] = reader.se()
    result["deblocking_filter_control_present"] = reader.read(1)
    result["constrained_intra_pred"] = reader.read(1)
    result["redundant_pic_cnt_present"] = reader.read(1)
    return result


def parse_avcc(extradata: bytes) -> tuple[int, list[bytes], list[bytes]]:
    if len(extradata) < 7 or extradata[0] != 1:
        raise AppleVideoContractError("invalid AVCDecoderConfigurationRecord")
    length_size = (extradata[4] & 3) + 1
    offset = 6
    sps_units: list[bytes] = []
    for _ in range(extradata[5] & 31):
        if offset + 2 > len(extradata):
            raise AppleVideoContractError("truncated avcC SPS length")
        size = int.from_bytes(extradata[offset:offset + 2], "big")
        offset += 2
        if not size or offset + size > len(extradata):
            raise AppleVideoContractError("invalid avcC SPS extent")
        sps_units.append(extradata[offset:offset + size])
        offset += size
    if offset >= len(extradata):
        raise AppleVideoContractError("truncated avcC PPS count")
    pps_units: list[bytes] = []
    count = extradata[offset]
    offset += 1
    for _ in range(count):
        if offset + 2 > len(extradata):
            raise AppleVideoContractError("truncated avcC PPS length")
        size = int.from_bytes(extradata[offset:offset + 2], "big")
        offset += 2
        if not size or offset + size > len(extradata):
            raise AppleVideoContractError("invalid avcC PPS extent")
        pps_units.append(extradata[offset:offset + size])
        offset += size
    if not sps_units or not pps_units:
        raise AppleVideoContractError("avcC has no SPS/PPS")
    return length_size, sps_units, pps_units


def decode_ffprobe_hex(value: object) -> bytes:
    output = bytearray()
    for line in str(value or "").splitlines():
        if ":" not in line:
            continue
        payload = line.split(":", 1)[1].split("  ", 1)[0]
        compact = "".join(re.findall(r"[0-9a-fA-F]+", payload))
        if compact and len(compact) % 2 == 0:
            output.extend(bytes.fromhex(compact))
    return bytes(output)


def _sample_nals(sample: bytes, length_size: int) -> list[bytes]:
    units: list[bytes] = []
    offset = 0
    while offset < len(sample):
        if offset + length_size > len(sample):
            raise AppleVideoContractError("truncated MP4 AVC sample length")
        size = int.from_bytes(sample[offset:offset + length_size], "big")
        offset += length_size
        if size <= 0 or offset + size > len(sample):
            raise AppleVideoContractError("invalid MP4 AVC NAL length")
        units.append(sample[offset:offset + size])
        offset += size
    return units


def _skip_ref_pic_list_modification(reader: _BitReader, slice_type: int) -> None:
    if slice_type not in {2, 4} and reader.read(1):
        raise AppleVideoContractError("unsupported reference-list modification")
    if slice_type == 1 and reader.read(1):
        raise AppleVideoContractError("unsupported B reference-list modification")


def _skip_dec_ref_pic_marking(reader: _BitReader, nal_type: int, nal_ref_idc: int) -> None:
    if not nal_ref_idc:
        return
    if nal_type == 5:
        reader.read(1)
        if reader.read(1):
            raise AppleVideoContractError("unsupported long-term reference")
    elif reader.read(1):
        raise AppleVideoContractError("unsupported adaptive reference marking")


def parse_slice(nal: bytes, sps: dict[str, int], pps: dict[str, int]) -> dict[str, int]:
    if not nal or nal[0] & 0x1F not in {1, 5}:
        raise AppleVideoContractError("NAL is not an H.264 picture slice")
    nal_type = nal[0] & 0x1F
    nal_ref_idc = (nal[0] >> 5) & 3
    reader = _BitReader(_rbsp(nal))
    result = {
        "nal_type": nal_type,
        "nal_ref_idc": nal_ref_idc,
        "first_mb_in_slice": reader.ue(),
    }
    slice_type = reader.ue() % 5
    result["slice_type"] = slice_type
    result["pps_id"] = reader.ue()
    result["frame_num"] = reader.read(sps["log2_max_frame_num_minus4"] + 4)
    if nal_type == 5:
        result["idr_pic_id"] = reader.ue()
    if sps["pic_order_cnt_type"] == 0:
        result["poc_lsb"] = reader.read(sps["log2_max_pic_order_cnt_lsb_minus4"] + 4)
        if pps["bottom_field_pic_order_in_frame_present"]:
            reader.se()
    if pps["redundant_pic_cnt_present"]:
        reader.ue()
    if slice_type == 1:
        reader.read(1)
    result["active_refs_minus1"] = pps["num_ref_idx_l0_default_active_minus1"]
    if slice_type in {0, 1, 3}:
        if reader.read(1):
            result["active_refs_minus1"] = reader.ue()
            if slice_type == 1:
                reader.ue()
    _skip_ref_pic_list_modification(reader, slice_type)
    if pps["weighted_pred"] and slice_type in {0, 3}:
        raise AppleVideoContractError("weighted prediction is outside the Apple contract")
    _skip_dec_ref_pic_marking(reader, nal_type, nal_ref_idc)
    if pps["entropy_coding_mode"] and slice_type not in {2, 4}:
        reader.ue()
    reader.se()
    if pps["deblocking_filter_control_present"]:
        result["disable_deblocking_filter_idc"] = reader.ue()
        if result["disable_deblocking_filter_idc"] != 1:
            reader.se()
            reader.se()
    return result


def inspect_avc_contract(
    path: str | os.PathLike[str],
    video_stream: dict[str, object],
    packets: list[dict[str, object]],
    max_packets: int | None = None,
) -> dict[str, object]:
    extradata = decode_ffprobe_hex(video_stream.get("extradata"))
    length_size, sps_units, pps_units = parse_avcc(extradata)
    sps = parse_sps(sps_units[0])
    pps = parse_pps(pps_units[0])
    stream_index = int(video_stream.get("index", -1))
    selected = [
        item for item in packets
        if int(item.get("stream_index", -2)) == stream_index
        and item.get("pos") is not None and item.get("size") is not None
    ]
    if max_packets is not None:
        selected = selected[:max_packets]
    nal_ref_idc: dict[str, set[int]] = {
        "7": {(sps_units[0][0] >> 5) & 3},
        "8": {(pps_units[0][0] >> 5) & 3},
    }
    picture_slices: list[list[dict[str, int]]] = []
    picture_nal_types: set[tuple[int, ...]] = set()
    sei_nals: set[str] = set()
    with Path(path).open("rb") as handle:
        for packet in selected:
            handle.seek(int(packet["pos"]))
            sample = handle.read(int(packet["size"]))
            slices: list[dict[str, int]] = []
            sample_nals = _sample_nals(sample, length_size)
            for nal in sample_nals:
                nal_type = nal[0] & 0x1F
                nal_ref_idc.setdefault(str(nal_type), set()).add((nal[0] >> 5) & 3)
                if nal_type in {1, 5}:
                    slices.append(parse_slice(nal, sps, pps))
                elif nal_type == 6:
                    sei_nals.add(nal.hex())
            if slices:
                picture_slices.append(slices)
                picture_nal_types.add(tuple(nal[0] & 0x1F for nal in sample_nals))
    return {
        "length_size": length_size,
        "sps": sps,
        "pps": pps,
        "nal_ref_idc": {
            key: sorted(values) for key, values in sorted(nal_ref_idc.items())
        },
        "pictures": len(picture_slices),
        "slices_per_picture": sorted({len(item) for item in picture_slices}),
        "nal_types_per_picture": [
            list(values) for values in sorted(picture_nal_types)
        ],
        "sei_nals": sorted(sei_nals),
        "first_mb_in_slice": [
            list(values) for values in sorted({
                tuple(part["first_mb_in_slice"] for part in item)
                for item in picture_slices
            })
        ],
        "disable_deblocking_filter_idc": sorted({
            part.get("disable_deblocking_filter_idc", -1)
            for item in picture_slices for part in item
        }),
    }


def apple_avc_contract_failures(
    contract: dict[str, object], width: int, height: int, level: int
) -> list[str]:
    failures: list[str] = []
    sps = dict(contract.get("sps") or {})
    pps = dict(contract.get("pps") or {})
    small_preset = level <= 13
    expected_sps = {
        "nal_ref_idc": 1,
        "profile_idc": 66,
        "constraint_flags": 0xE0,
        "level_idc": level,
        "log2_max_frame_num_minus4": 1,
        "pic_order_cnt_type": 0,
        "log2_max_pic_order_cnt_lsb_minus4": 3,
        "max_num_ref_frames": 2 if small_preset else 1,
        "coded_width": ((width + 15) // 16) * 16,
        "coded_height": ((height + 15) // 16) * 16,
        "width": width,
        "height": height,
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
        "vcl_hrd_parameters_present": 0,
        "low_delay_hrd": 0,
        "pic_struct_present": 0,
        "bitstream_restriction": 0,
    }
    for field, expected in expected_sps.items():
        if sps.get(field) != expected:
            failures.append(
                f"SPS {field} differs: expected {expected!r}, got {sps.get(field)!r}"
            )
    expected_pps = {
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
        "deblocking_filter_control_present": 0 if small_preset else 1,
        "constrained_intra_pred": 0,
        "redundant_pic_cnt_present": 0,
    }
    for field, expected in expected_pps.items():
        if pps.get(field) != expected:
            failures.append(
                f"PPS {field} differs: expected {expected!r}, got {pps.get(field)!r}"
            )
    expected_hrd = {
        "cpb_count": 1,
        "bit_rate_scale": 7,
        "cpb_size_scale": 10,
        "bit_rate_value_minus1": [93] if small_preset else [488],
        "cpb_size_value_minus1": [121] if small_preset else [244],
        "cbr_flag": [0],
        "initial_cpb_removal_delay_length_minus1": 23,
        "cpb_removal_delay_length_minus1": 23,
        "dpb_output_delay_length_minus1": 23,
        "time_offset_length": 24,
    }
    hrd = dict(sps.get("nal_hrd") or {})
    for field, expected in expected_hrd.items():
        if hrd.get(field) != expected:
            failures.append(
                f"SPS NAL-HRD {field} differs: expected {expected!r}, "
                f"got {hrd.get(field)!r}"
            )
    if contract.get("length_size") != 4:
        failures.append("AVC samples do not use four-byte NAL lengths")
    for nal_type in ("5", "7", "8"):
        values = (contract.get("nal_ref_idc") or {}).get(nal_type)
        if values is not None and values != [1]:
            failures.append(f"NAL type {nal_type} reference priority is not exactly 1")
    expected_p_refs = [1] if small_preset else [0, 1]
    if (contract.get("nal_ref_idc") or {}).get("1") != expected_p_refs:
        failures.append(
            f"P-slice reference pattern differs: expected {expected_p_refs!r}"
        )
    if contract.get("pictures", 0) <= 0:
        failures.append("no H.264 pictures were inspected")
    if contract.get("slices_per_picture") != [2]:
        failures.append("pictures do not contain exactly two slices")
    expected_nal_types = (
        [[1, 1], [6, 5, 5]] if small_preset
        else [[6, 1, 1], [6, 6, 5, 5]]
    )
    if contract.get("nal_types_per_picture") != expected_nal_types:
        failures.append("MP4 sample NAL layout differs from Apple")
    expected_sei = (
        ["06000781f63b8000004080"] if small_preset else
        [
            "0600078493e0000003004080",
            "0605110387f44ecd0a4bdca1943ac3d49b171f0180",
        ]
    )
    if contract.get("sei_nals") != expected_sei:
        failures.append("SEI payloads differ from Apple")
    total_mbs = ((width + 15) // 16) * ((height + 15) // 16)
    if contract.get("first_mb_in_slice") != [[0, total_mbs // 2]]:
        failures.append("two-slice macroblock split differs from Apple")
    expected_deblock = [-1] if small_preset else [1]
    if contract.get("disable_deblocking_filter_idc") != expected_deblock:
        failures.append("slice deblocking mode differs from Apple")
    return failures
