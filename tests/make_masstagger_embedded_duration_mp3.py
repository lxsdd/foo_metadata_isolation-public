"""Copyright-free ~54-second *two-track* embedded Cue for real Masstagger UI.

The shorter fixture is used by unit/structural tests, but its second INDEX 01
at 25 seconds is past the ~0.89-second MPEG seed. The original Masstagger
native UI may initialize a decoder/preview. Extend ONLY this disposable
Masstagger UI fixture, never alter baseline fixtures or any user media.
"""
import argparse
import hashlib
from pathlib import Path
from make_isolated_embedded_mp3 import create, audio

REPEATS = 60

def make(path: Path) -> None:
    create(path)  # Refuses existing file, fixes original ID3/PRIV/CUE seed.
    seed = audio()
    with path.open("ab") as target:
        for _ in range(REPEATS):
            target.write(seed)
    seconds = (REPEATS+1)*34*1152/44100
    assert seconds > 40 and path.stat().st_size > 200000
    print(f"MASSTAGGER_CUE_MEDIA_DURATION_SECONDS={seconds:.3f}")
    print(f"MASSTAGGER_CUE_MEDIA_SHA256={hashlib.sha256(path.read_bytes()).hexdigest()}")

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("target",type=Path)
    make(parser.parse_args().target)
