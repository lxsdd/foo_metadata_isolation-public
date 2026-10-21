"""Independent full-byte oracle for an actual production timing-dialog Commit.

The caller can pass only disposable, generated media.  This oracle deliberately
does not trust the component, any diagnostic report, or an exit code.
"""
import argparse
from pathlib import Path
from assert_isolated_virtual_write import parse

ORIGINAL = b"    INDEX 01 00:00:30"
CHANGED = b"    INDEX 01 00:00:35"

def verify(before: Path, after: Path) -> None:
    original = before.read_bytes()
    actual = after.read_bytes()
    original_frames, original_cue, original_audio = parse(before)
    actual_frames, actual_cue, actual_audio = parse(after)
    assert original_frames == actual_frames, "physical ID3/opaque PRIV changed"
    assert original_audio == actual_audio, "MPEG audio changed"
    assert original_cue.count(ORIGINAL) == 1, "original INDEX 01 not unique"
    assert actual_cue == original_cue.replace(ORIGINAL, CHANGED), "raw Cue non-target bytes modified"
    assert original_cue.count(b"  TRACK ") == actual_cue.count(b"  TRACK ") == 3
    assert b"REM UNIQUE_KEEP exactly-preserved" in actual_cue
    assert original != actual, "UI Commit was a no-op"
    assert original.count(ORIGINAL) == 1, "unexpected duplicate raw INDEX marker"
    assert actual == original.replace(ORIGINAL, CHANGED, 1), (
        "UI Commit altered bytes outside the exact raw-Cue time token"
    )

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    args = parser.parse_args()
    verify(args.before, args.after)
    print("PRODUCTION_VIRTUAL_TIMING_REAL_UI_COMMIT_BYTE_EXACT=PASS")

if __name__ == "__main__":
    main()
