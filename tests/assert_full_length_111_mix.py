"""Streaming independent integrity oracle for ~193 MiB 111-track synthetic Mix.

No full audio is read into memory: hash all MPEG payload bytes in chunks.
This requires the actual real-foobar benchmark report, non-no-op virtual
CUE TITLE changes and physical tags/opaque PRIV/audio preservation.
"""
import argparse
import hashlib
import re
from pathlib import Path

from assert_isolated_virtual_write import ss, without_virtual_title_values
from make_full_length_111_mix import (
    MPEG_AUDIO_BYTES, TRACK_COUNT, TRACK_INTERVAL_SECONDS, cue_bytes,
)

REQUIRED = (
    ("production_virtual_track_count", str(TRACK_COUNT)),
    ("benchmark_selection_count", str(TRACK_COUNT)),
    ("benchmark_prepared", "true"),
    ("benchmark_committed", "true"),
    ("benchmark_result", "PASS"),
    ("write_result", "benchmark_pass"),
)


def inspect(path: Path):
    with path.open("rb") as source:
        header = source.read(10)
        assert header[:6] == b"ID3\x04\x00\x00", "unexpected ID3 version/flags"
        tag_length = ss(header[6:10])
        assert 100 < tag_length < 1024 * 1024, "unbounded ID3 size"
        tag = source.read(tag_length)
        assert len(tag) == tag_length, "truncated ID3"
        physical, pos = {}, 0
        while pos + 10 <= tag_length and tag[pos:pos+4] != b"\0\0\0\0":
            name = tag[pos:pos+4].decode("ascii")
            assert re.fullmatch(r"[A-Z0-9]{4}", name), name
            count = ss(tag[pos+4:pos+8])
            assert tag[pos+8:pos+10] == b"\0\0"
            payload = tag[pos+10:pos+10+count]
            assert len(payload) == count and pos + 10 + count <= tag_length
            physical.setdefault(name, []).append(payload)
            pos += 10 + count
        assert set(physical) == {"TIT2", "TPE1", "TALB", "TCON", "PRIV", "TXXX"}
        assert len(physical["PRIV"]) == len(physical["TXXX"]) == 1
        cue_payload = physical.pop("TXXX")[0]
        assert cue_payload.startswith(b"\x03CUESHEET\0")
        cue = cue_payload[len(b"\x03CUESHEET\0"):]
        start_audio = source.tell()
        first_frame = source.read(4)
        assert first_frame == b"\xff\xfb\xe0\xc4", "not synthetic 320 kbit/s MPEG-1 layer III"
        source.seek(start_audio)
        digest = hashlib.sha256()
        length = 0
        while block := source.read(1024 * 1024):
            digest.update(block)
            length += len(block)
        return physical, cue, digest.digest(), length


def main():
    p = argparse.ArgumentParser()
    p.add_argument("before", type=Path)
    p.add_argument("after", type=Path)
    p.add_argument("report", type=Path)
    args = p.parse_args()
    text = args.report.read_text(encoding="utf-8-sig")
    for key, value in REQUIRED:
        assert re.search(r"(?m)^" + re.escape(key + "=" + value) + r"\s*$", text), (key, value)
    btags, bcue, bhash, bsize = inspect(args.before)
    atags, acue, ahash, asize = inspect(args.after)
    assert bsize == asize == MPEG_AUDIO_BYTES, "MPEG byte count changed"
    assert bhash == ahash, "MPEG audio SHA-256 changed"
    assert btags == atags, "physical ID3 tags or opaque PRIV modified"
    assert bcue == cue_bytes(), "initial cue index positions or headers differ"
    assert without_virtual_title_values(bcue) == without_virtual_title_values(acue), "unrelated CUE tokens modified"
    old_titles = re.findall(rb'(?m)^    TITLE "([^"\r\n]*)"', bcue)
    new_titles = re.findall(rb'(?m)^    TITLE "([^"\r\n]*)"', acue)
    assert len(old_titles) == len(new_titles) == TRACK_COUNT
    assert all(a != b for a, b in zip(old_titles, new_titles)), "some virtual TITLE values not changed"
    assert acue.count(b"INDEX 01") == TRACK_COUNT
    for i in range(TRACK_COUNT):
        seconds = i * TRACK_INTERVAL_SECONDS
        index = f"INDEX 01 {seconds // 60:02d}:{seconds % 60:02d}:00".encode()
        assert acue.count(index) == 1, f"wrong original timing at cue track {i+1}"
    metrics = dict(re.findall(r"(?m)^(benchmark_[a-z_]+)=(\d+)\s*$", text))
    for key in ("benchmark_total_us", "benchmark_process_read_bytes",
                "benchmark_process_write_bytes", "benchmark_source_allocated_bytes",
                "benchmark_shadow_allocated_bytes"):
        assert key in metrics, "missing observed " + key
    source_bytes = int(metrics["benchmark_source_allocated_bytes"])
    shadow_bytes = int(metrics["benchmark_shadow_allocated_bytes"])
    writes = int(metrics["benchmark_process_write_bytes"])
    assert source_bytes >= MPEG_AUDIO_BYTES
    assert shadow_bytes <= source_bytes * 4, "unbounded shadow disk amplification"
    assert writes <= source_bytes * 20, "N-by-full-file write amplification"
    print("REAL_HOST_FULL_LENGTH_111_RAW_CUE_ID3_PRIV_MPEG_STREAMING_ORACLE=PASS")
    print(f"MI_FULL_111_MPEG_AUDIO_SHA256={bhash.hex()}")
    print(f"MI_FULL_111_MPEG_AUDIO_BYTES={bsize}")
    for key in sorted(metrics):
        if key.endswith(("_us", "_bytes")):
            print(f"MI_FULL_111_{key.upper()}={metrics[key]}")


if __name__ == "__main__":
    main()
