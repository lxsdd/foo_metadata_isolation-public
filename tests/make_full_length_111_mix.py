"""Copyright-free, deterministic, full-length 111-track DJ Mix for real Windows E2E.

Creates an ~84-minute / 193-MiB synthetic MP3 (MPEG-1 Layer III, 44.1 kHz,
320 kbit/s) with an embedded ID3v2.4 CUE. No copyrighted recordings,
network media, external encoders, credentials or user's music required.

The seed is the repository's byte-pinned synthetic MPEG silence. Frames
preserve valid MPEG headers/side information and change only the bitrate
index and frame-padding length. The resulting short segment decodes using
FFmpeg, and large files are written in bounded chunks.
"""
import argparse
import hashlib
from pathlib import Path

from make_isolated_embedded_mp3 import audio, frame, syncsafe

TRACK_COUNT = 111
TRACK_INTERVAL_SECONDS = 45
REPEATS = 5700
FRAMES_PER_REPEAT = 34
MP3_FRAME_BYTES = 1044
MPEG_SEGMENT_BYTES = 35513
MPEG_AUDIO_BYTES = MPEG_SEGMENT_BYTES * REPEATS


def high_bitrate_silence() -> bytes:
    original = audio()
    frames = []
    pos = 0
    for i in range(FRAMES_PER_REPEAT):
        padded = i % 2
        length = 104 + padded
        original_frame = bytearray(original[pos:pos + length])
        assert len(original_frame) == length
        assert original_frame[:2] == b"\xff\xfb"
        assert original_frame[2] == (0x12 if padded else 0x10)
        assert original_frame[3] == 0xc4
        # MPEG-1 layer III: bitrate index 14 = 320 kb/s (formerly 1 = 32).
        original_frame[2] = 0xe2 if padded else 0xe0
        original_frame.extend(b"\0" * (MP3_FRAME_BYTES + padded - length))
        frames.append(original_frame)
        pos += length
    assert pos == len(original)
    result = b"".join(frames)
    assert len(result) == MPEG_SEGMENT_BYTES
    return result


def cue_bytes() -> bytes:
    lines = [
        'PERFORMER "Synthetic Global Artist"',
        'TITLE "Synthetic Copyright-Free Continuous 111 Track Mix"',
        'REM GENRE Trance',
        'REM DATE 2026',
        'REM UNIQUE_KEEP exactly-preserved',
        'FILE "track-01.mp3" MP3',
    ]
    for track in range(1, TRACK_COUNT + 1):
        seconds = (track - 1) * TRACK_INTERVAL_SECONDS
        lines.extend((
            f'  TRACK {track:02d} AUDIO',
            f'    TITLE "Virtual Original {track:03d}"',
            '    PERFORMER "Synthetic Artist"',
            f'    INDEX 01 {seconds // 60:02d}:{seconds % 60:02d}:00',
        ))
    cue = ("\r\n".join(lines) + "\r\n").encode("utf-8")
    assert cue.count(b"INDEX 01") == TRACK_COUNT
    assert b"INDEX 01 82:30:00" in cue
    return cue


def create(target: Path) -> None:
    if target.name != "track-01.mp3" or target.exists():
        raise ValueError("Only a new disposable track-01.mp3 is supported")
    cue = cue_bytes()
    physical = b"".join((
        frame("TIT2", b"\x03Physical Original Title"),
        frame("TPE1", b"\x03Physical Original Artist"),
        frame("TALB", b"\x03Physical Original Album"),
        frame("TCON", b"\x03Trance"),
        frame("PRIV", b"metadata-isolation-probe\0\x05\x00opaque\xfe\x00"),
        frame("TXXX", b"\x03CUESHEET\0" + cue),
    ))
    head = b"ID3\x04\x00\x00" + syncsafe(len(physical)) + physical
    segment = high_bitrate_silence()
    digest = hashlib.sha256()
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("xb") as output:
        output.write(head)
        digest.update(head)
        group = segment * 32
        for _ in range(REPEATS // 32):
            output.write(group)
            digest.update(group)
        tail = segment * (REPEATS % 32)
        if tail:
            output.write(tail)
            digest.update(tail)
    assert target.stat().st_size == len(head) + MPEG_AUDIO_BYTES
    print(f"SYNTHETIC_FULL_111_AUDIO_BITRATE=320000")
    print(f"SYNTHETIC_FULL_111_MPEG_AUDIO_BYTES={MPEG_AUDIO_BYTES}")
    print(f"SYNTHETIC_FULL_111_TRACK_COUNT={TRACK_COUNT}")
    print(f"SYNTHETIC_FULL_111_DURATION_SECONDS_APPROX={REPEATS * FRAMES_PER_REPEAT * 1152 / 44100:.2f}")
    print(f"SYNTHETIC_FULL_111_FILE_SHA256={digest.hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("target", type=Path)
    create(parser.parse_args().target)
