"""Deterministic copyright-free MPEG audio with a two-track embedded CUE.

Only create inside a disposable GitHub Actions runner; never overwrite.
"""
import argparse
import hashlib
from pathlib import Path

EXPECTED_AUDIO_SHA = "81f0ef1a0f124c413648c525d7118e4f3d8f8051c6724c7ff74ac6bdb5e3d114"

def syncsafe(n):
    if not 0 <= n <= 0x0FFFFFFF:
        raise ValueError("Invalid tag size")
    return bytes((n >> 21 & 127, n >> 14 & 127, n >> 7 & 127, n & 127))

def frame(name, payload):
    return name.encode("ascii") + syncsafe(len(payload)) + b"\0\0" + payload

def audio():
    common = bytes.fromhex("c00001a40000002000003480000004")
    p4 = [0,41,83,125,167,208,214,213]
    p5 = [3,131,131,3,3,131,3,131]
    lame = b"LAME3.100"
    result = bytearray()
    for i in range(34):
        odd = i % 2
        length = 104 + odd
        data = bytearray(b"U" * length)
        slot = i if i < 8 else 6 + odd
        data[0:6] = bytes((255,251,18 if odd else 16,196,p4[slot],p5[slot]))
        data[6:6+len(common)] = common
        if i == 0:
            data[21:21+len(lame)] = lame
        if i < 28:
            data[-10:-1] = lame
        result += data
    assert len(result) == 3553
    assert hashlib.sha256(result).hexdigest() == EXPECTED_AUDIO_SHA
    return bytes(result)

def create(destination):
    if destination.name != "track-01.mp3" or destination.exists():
        raise RuntimeError("Invalid or existing disposable fixture target")
    cue = "\r\n".join((
        'PERFORMER "Cue Global Artist"',
        'TITLE "Cue Global Album"',
        'REM GENRE Trance',
        'REM DATE 2026',
        'REM UNIQUE_KEEP exactly-preserved',
        'FILE "track-01.mp3" MP3',
        '  TRACK 01 AUDIO',
        '    TITLE "Virtual Original One"',
        '    PERFORMER "Virtual Artist One"',
        '    INDEX 01 00:00:00',
        '  TRACK 02 AUDIO',
        '    TITLE "Virtual Original Two"',
        '    PERFORMER "Virtual Artist Two"',
        '    INDEX 01 00:00:25',
        '',
    )).encode("utf-8")
    tag = b"".join((
        frame("TIT2", b"\x03Physical Original Title"),
        frame("TPE1", b"\x03Physical Original Artist"),
        frame("TALB", b"\x03Physical Original Album"),
        frame("TCON", b"\x03Trance"),
        frame("PRIV", b"metadata-isolation-probe\0\x05\x00opaque\xfe\x00"),
        frame("TXXX", b"\x03CUESHEET\0" + cue),
    ))
    data = b"ID3\x04\x00\x00" + syncsafe(len(tag)) + tag + audio()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("xb") as output:
        output.write(data)
    print("EMBEDDED_CUE_FIXTURE=GENERATED", hashlib.sha256(data).hexdigest())

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("target", type=Path)
    create(parser.parse_args().target)
