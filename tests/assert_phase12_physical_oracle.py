"""Independent Phase-12 container and fresh-reload oracle.

The oracle accepts host normalization of the fields deliberately written by the
probe.  It asserts exact preservation only for unrelated carrier state.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from dump_mp3 import dump as dump_mp3
from assert_flac_native_cuesheet import metadata_blocks, vorbis_comment_names
from mutagen.flac import FLAC


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def flac_dump(path: Path) -> dict:
    raw = path.read_bytes()
    blocks = metadata_blocks(raw)
    audio_offset = blocks[-1].end_offset
    tags = FLAC(path)
    return {
        "blocks": [block.block_type for block in blocks],
        "block_bytes": [digest(raw[block.header_offset:block.end_offset]) for block in blocks],
        "audio": digest(raw[audio_offset:]),
        "tags": {key.upper(): list(values) for key, values in tags.items()},
        "pictures": [digest(picture.data) for picture in tags.pictures],
        "comment_names": [
            list(vorbis_comment_names(raw[b.payload_offset:b.end_offset]))
            for b in blocks if b.block_type == 4
        ],
    }


def assert_matrix(tags: dict) -> None:
    assert tags.get("TITLE", tags.get("TIT2")) == ["Physical Matrix Title Δ"]
    assert tags.get("DISCNUMBER", tags.get("TPOS")) == ["2"]
    assert tags["PHASE12_ADD"] == ["added 東京"]
    assert tags["CUSTOM_FIELD"] == ["changed Ω"]
    assert tags["MUSICBRAINZ_RELEASEID"] == ["f6758afb-ecff-410e-8979-b131cfa3c6d3"]
    assert "MULTI_VALUE_FIELD" not in tags
    assert "COMMENT" not in tags
    # The host writer normalizes away an empty fifth member; the four
    # non-empty values remain ordered and duplicate-preserving.
    assert tags["PHASE12_VECTOR"] == ["α", "β", "α", "😀"]


def id3_key(frame: dict) -> str:
    frame_id = frame["id"].upper()
    if frame_id == "TXXX":
        return f"TXXX:{frame.get('desc', '').casefold()}"
    if frame_id == "COMM":
        return f"COMM:{frame.get('desc', '').casefold()}:{frame.get('lang', '').casefold()}"
    if frame_id == "APIC":
        return f"APIC:{frame.get('desc', '').casefold()}"
    if frame_id == "PRIV":
        return f"PRIV:{frame.get('owner', '').casefold()}"
    return frame_id


def mp3_preserved(before: dict, after: dict, expect: str) -> None:
    assert before["audio_payload"]["sha256"] == after["audio_payload"]["sha256"]
    assert before["apev2"] == after["apev2"]
    assert before["id3v1"] == after["id3v1"]
    assert before["cuesheet"]["text"] == after["cuesheet"]["text"]
    targets = {
        "TIT2", "TPOS", "COMM::eng", "TXXX:phase12_add", "TXXX:custom_field", "TXXX:multi_value_field",
        "TXXX:phase12_vector", "TXXX:musicbrainz_releaseid",
        "TXXX:replaygain_track_gain", "TXXX:replaygain_track_peak",
        "TXXX:replaygain_album_gain", "TXXX:replaygain_album_peak",
    }
    before_frames = {id3_key(frame): frame for frame in before["id3v2"]["frames"]}
    after_frames = {id3_key(frame): frame for frame in after["id3v2"]["frames"]}
    assert {key: value for key, value in before_frames.items() if key not in targets} == {
        key: value for key, value in after_frames.items() if key not in targets
    }
    tags = {frame.get("desc", frame["id"]).upper(): frame.get("values", []) for frame in after["id3v2"]["frames"]}
    if expect == "matrix":
        assert_matrix(tags)
        assert not any(frame["id"].upper() == "COMM" for frame in after["id3v2"]["frames"])
    else:
        assert not any(key.startswith("REPLAYGAIN_") for key in tags)


def flac_preserved(before: dict, after: dict, native: bool, expect: str) -> None:
    assert before["audio"] == after["audio"]
    assert before["pictures"] == after["pictures"]
    assert before["blocks"].count(5) == after["blocks"].count(5)
    for block_type in (0, 5, 6):
        old_blocks = [
            digest for kind, digest in zip(before["blocks"], before["block_bytes"])
            if kind == block_type
        ]
        new_blocks = [
            digest for kind, digest in zip(after["blocks"], after["block_bytes"])
            if kind == block_type
        ]
        assert old_blocks == new_blocks
    if native:
        index = before["blocks"].index(5)
        assert before["block_bytes"][index] == after["block_bytes"][index]
    targets = {
        "TITLE", "DISCNUMBER", "COMMENT", "PHASE12_ADD", "CUSTOM_FIELD", "MUSICBRAINZ_RELEASEID",
        "MULTI_VALUE_FIELD", "PHASE12_VECTOR",
        "REPLAYGAIN_TRACK_GAIN", "REPLAYGAIN_TRACK_PEAK",
        "REPLAYGAIN_ALBUM_GAIN", "REPLAYGAIN_ALBUM_PEAK",
    }
    assert {key: value for key, value in before["tags"].items() if key not in targets} == {
        key: value for key, value in after["tags"].items() if key not in targets
    }
    if expect == "matrix":
        assert_matrix(after["tags"])
    else:
        assert not any(key.startswith("REPLAYGAIN_") for key in after["tags"])


def read_report(path: Path) -> dict[str, str]:
    output: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            output[key] = value
    return output


def decode_hex(value: str) -> str:
    return bytes.fromhex(value).decode("utf-8")


def host_inventory(data: dict[str, str], target: int) -> tuple[dict[str, list[str]], str]:
    prefix = f"target_{target}_after_track_0"
    fields: dict[str, list[str]] = {}
    for field in range(int(data[f"{prefix}_field_count"])):
        name = decode_hex(data[f"{prefix}_field_{field}_name_hex"]).upper()
        count = int(data[f"{prefix}_field_{field}_value_count"])
        fields[name] = [
            decode_hex(data[f"{prefix}_field_{field}_value_{value}_hex"])
            for value in range(count)
        ]
    return fields, prefix


def assert_host_postimage(report: Path, target: int, expect: str) -> None:
    data = read_report(report)
    fields, prefix = host_inventory(data, target)
    if expect == "matrix":
        assert_matrix(fields)
        expected_rg = {
            "track_gain": "-6.25 dB",
            "track_peak": "0.712345",
            "album_gain": "-8.75 dB",
            "album_peak": "0.912345",
        }
        for component, formatted in expected_rg.items():
            assert data[f"{prefix}_rg_{component}_present"] == "true"
            assert decode_hex(data[f"{prefix}_rg_{component}_formatted_hex"]) == formatted
    else:
        for component in ("track_gain", "track_peak", "album_gain", "album_peak"):
            assert data[f"{prefix}_rg_{component}_present"] == "false"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--format", choices=("mp3", "flac"), required=True)
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--expect", choices=("matrix", "remove"), default="matrix")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--target-index", type=int)
    args = parser.parse_args()
    if args.format == "mp3":
        mp3_preserved(dump_mp3(args.before), dump_mp3(args.after), args.expect)
    else:
        flac_preserved(flac_dump(args.before), flac_dump(args.after), args.native, args.expect)
    if (args.report is not None or args.target_index is not None):
        assert args.report is not None and args.target_index is not None
        assert_host_postimage(args.report, args.target_index, args.expect)
    print("Phase-12 independent container and reload oracle passed.")
