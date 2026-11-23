"""Generate disposable copyright-free plain ID3v2 MP3 for real Masstagger CLI.

The original synthetic two-subtrack fixture is *never* modified. We create
an independent new file removing its CUESHEET text frame (no virtual subsongs).
The remaining physical ID3/opaque PRIV and MPEG bytes remain intact.
"""
import argparse
from pathlib import Path
from make_isolated_embedded_mp3 import audio, frame, syncsafe
def main(path: Path):
    if path.name != "track-01.mp3" or path.exists():
        raise ValueError("Refuse a non-unique synthetic file target")
    frames = [
        frame("TIT2", b"\x03Physical Original Title"),
        frame("TPE1", b"\x03Physical Original Artist"),
        frame("TALB", b"\x03Physical Original Album"),
        frame("TCON", b"\x03Trance"),
        frame("PRIV", b"metadata-isolation-probe\0\x05\x00opaque\xfe\x00"),
    ]
    t=b"".join(frames)
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open("xb") as f:
        f.write(b"ID3\x04\x00\x00"+syncsafe(len(t))+t+audio())
    print("MASSTAGGER_SYNTHETIC_PHYSICAL_NO_CUE=PASS")
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("target",type=Path)
    a=p.parse_args()
    main(a.target)
