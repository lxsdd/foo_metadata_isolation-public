"""Independent negative control: native direct writer MUST NOT change embedded Cue.

Refuse success without proof that the command actually targeted subsong 1,
attempted a different TITLE, and foobar's guard callback was observed.
"""
import argparse
from pathlib import Path
import re
from assert_isolated_virtual_write import parse

def require(report, key, expected):
    match = re.search(r"(?m)^" + re.escape(key) + r"=(.*?)\\r?$", report)
    assert match is not None, f"missing writer evidence: {key}"
    assert match.group(1) == expected, f"{key}: expected {expected!r}, got {match.group(1)!r}"

def main():
    p = argparse.ArgumentParser()
    p.add_argument("original", type=Path)
    p.add_argument("after", type=Path)
    p.add_argument("report", type=Path)
    args = p.parse_args()
    report = args.report.read_text(encoding="utf-8-sig")
    require(report, "direct_guard_subsong", "1")
    require(report, "direct_guard_write_attempted", "true")
    require(report, "direct_guard_title_before", "Virtual Original One")
    require(report, "direct_guard_title_attempted", "Direct Unsafe Probe MUST NOT COMMIT")
    require(report, "direct_guard_title_after", "Virtual Original One")
    require(report, "direct_guard_protection_after", "ACTIVE")
    require(report, "write_result", "guard_preserved")
    assert args.original.read_bytes() == args.after.read_bytes(), "direct virtual writer modified original bytes"
    original_frames, original_cue, original_audio = parse(args.original)
    actual_frames, actual_cue, actual_audio = parse(args.after)
    assert original_frames == actual_frames, "physical fields/PRIV changed"
    assert original_cue == actual_cue, "embedded Cue changed"
    assert original_audio == actual_audio, "MPEG changed"
    print("REAL_FOOBAR_DIRECT_GUARD=PASS; confirmed write attempt, observed Guard, source byte-identical")

if __name__ == "__main__":
    main()
