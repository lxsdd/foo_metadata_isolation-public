"""Synthetic 3-track MP3 Embedded CUE with realistic 30s/60s boundaries."""
from pathlib import Path
import argparse
import hashlib
from make_isolated_embedded_mp3 import audio, frame, syncsafe

def main(destination):
    if destination.name != "track-01.mp3" or destination.exists():
        raise RuntimeError("Fixture target must be a new track-01.mp3")
    cue = ("\r\n".join([
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
        '    INDEX 01 00:30:00',
        '  TRACK 03 AUDIO',
        '    TITLE "Virtual Original Three"',
        '    PERFORMER "Virtual Artist Three"',
        '    INDEX 01 01:00:00',
        '',
    ])).encode("utf-8")
    tag=b"".join([
        frame("TIT2", b"\x03Physical Original Title"),
        frame("TPE1", b"\x03Physical Original Artist"),
        frame("TALB", b"\x03Physical Original Album"),
        frame("TCON", b"\x03Trance"),
        frame("PRIV", b"metadata-isolation-probe\0\x05\x00opaque\xfe\x00"),
        frame("TXXX", b"\x03CUESHEET\0"+cue),
    ])
    mpeg=audio()*160
    blob=b"ID3\x04\x00\x00"+syncsafe(len(tag))+tag+mpeg
    destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open("xb") as out:
        out.write(blob)
    print("TIMING_FIXTURE_SHA256="+hashlib.sha256(blob).hexdigest())
    print("TIMING_FIXTURE_AUDIO_BYTES="+str(len(mpeg)))

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("target",type=Path)
    args=p.parse_args()
    main(args.target)
