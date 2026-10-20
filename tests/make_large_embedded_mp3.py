"""Deterministic 111-subtrack embedded-Cue fixture for real foobar CI.

Derived only from the repository's synthetic MPEG silence. No personal media.
"""
import argparse
import hashlib
from pathlib import Path
from make_isolated_embedded_mp3 import audio, frame, syncsafe

TRACK_COUNT = 111
AUDIO_REPEATS = 160  # ~142 seconds of synthetic MPEG frames, over 110 s Cue span.

def create(path):
    if path.name != "track-01.mp3" or path.exists():
        raise RuntimeError("Expected a new track-01.mp3 in runner temp")
    lines = [
        'PERFORMER "Synthetic Global Artist"',
        'TITLE "Synthetic 111 Track Album"',
        'REM GENRE Trance',
        'REM DATE 2026',
        'REM UNIQUE_KEEP exactly-preserved',
        'FILE "track-01.mp3" MP3'
    ]
    for track in range(1, TRACK_COUNT + 1):
        pos = track - 1
        lines.extend((
            f'  TRACK {track:02d} AUDIO',
            f'    TITLE "Virtual Original {track:03d}"',
            f'    PERFORMER "Synthetic Artist"',
            f'    INDEX 01 {pos // 60:02d}:{pos % 60:02d}:00'
        ))
    cue = ("\r\n".join(lines) + "\r\n").encode("utf-8")
    physical = b"".join((
        frame("TIT2", b"\x03Physical Original Title"),
        frame("TPE1", b"\x03Physical Original Artist"),
        frame("TALB", b"\x03Physical Original Album"),
        frame("TCON", b"\x03Trance"),
        frame("PRIV", b"metadata-isolation-probe\0\x05\x00opaque\xfe\x00"),
        frame("TXXX", b"\x03CUESHEET\0" + cue)
    ))
    mpeg = audio() * AUDIO_REPEATS
    blob = b"ID3\x04\x00\x00" + syncsafe(len(physical)) + physical + mpeg
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as dst:
        dst.write(blob)
    print(f"SYNTHETIC_111_FIXTURE_SHA256={hashlib.sha256(blob).hexdigest()}")
    print(f"SYNTHETIC_111_FIXTURE_AUDIO_BYTES={len(mpeg)}")
    print(f"SYNTHETIC_111_FIXTURE_TRACK_COUNT={TRACK_COUNT}")

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("target", type=Path)
    create(p.parse_args().target)
