"""Fail-closed oracle for the real foobar physical-metadata classifier probe.

Only synthetic CI files are accepted; this never opens user media or writes tags.
The native C++ classifier must emit all expected state/missing/review fields.
"""
import argparse
import hashlib
from pathlib import Path


EXPECTED = {
    "ok": "OK",
    "artist_fallback": "OK",
    "empty": "EMPTY",
    "incomplete": "INCOMPLETE",
    "whitespace": "INCOMPLETE",
    "mismatch": "MISMATCH",
    "review": "REVIEW",
    "review_incomplete": "REVIEW",
    "review_empty_cue_ambiguous": "REVIEW",
    "mismatch_incomplete": "INCOMPLETE",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("fixture", type=Path)
    args = parser.parse_args()

    require(args.fixture.name == "track-01.mp3", "Unrecognized fixture")
    require(args.baseline.name == "track-01.mp3.before", "Unrecognized baseline")
    require(args.report.name == "track-01.mp3.metadata-isolation-probe.txt", "Unrecognized report")
    require(args.report.parent == args.fixture.parent == args.baseline.parent, "Nonlocal probe inputs")
    require(sha256(args.fixture) == sha256(args.baseline), "Read-only classifier changed synthetic media")

    rows = {}
    for line in args.report.read_text(encoding="utf-8-sig").splitlines():
        if not line.strip():
            continue
        require("=" in line, "Malformed classifier report")
        key, value = line.split("=", 1)
        require(key not in rows, "Duplicate classifier report key")
        rows[key] = value

    require(rows.get("write_result") == "read_only_classifier", "Wrong real host probe")
    expected_keys = {"write_result"}
    for case, status in EXPECTED.items():
        for suffix in ("status", "missing", "differences", "review"):
            expected_keys.add(f"{case}_{suffix}")
        require(rows.get(f"{case}_status") == status, f"{case}: wrong production classifier status")
    require(set(rows) == expected_keys, "Missing or unexpected real-host audit classifier outputs")

    def contains(case: str, suffix: str, phrase: str) -> None:
        require(phrase in rows[f"{case}_{suffix}"], f"{case}: missing {suffix} evidence")

    def empty(case: str, suffix: str) -> None:
        require(rows[f"{case}_{suffix}"] == "", f"{case}: unexpected {suffix}")

    for case in ("ok", "artist_fallback", "empty", "incomplete", "whitespace",
                 "mismatch", "review", "review_incomplete",
                 "review_empty_cue_ambiguous", "mismatch_incomplete"):
        if case not in ("review", "review_incomplete", "review_empty_cue_ambiguous"):
            empty(case, "review")
    for case in ("ok", "artist_fallback", "empty", "incomplete", "whitespace",
                 "review", "review_incomplete", "review_empty_cue_ambiguous"):
        empty(case, "differences")
    for case in ("ok", "artist_fallback", "mismatch", "review"):
        empty(case, "missing")
    contains("empty", "missing", "ARTIST / ALBUM ARTIST")
    contains("empty", "missing", "DATE")
    contains("incomplete", "missing", "GENRE")
    contains("whitespace", "missing", "DATE")
    contains("mismatch", "differences", "GENRE")
    contains("review", "review", "GENRE")
    contains("review_incomplete", "missing", "DATE")
    contains("review_incomplete", "review", "GENRE")
    contains("review_empty_cue_ambiguous", "missing", "DATE")
    contains("review_empty_cue_ambiguous", "review", "GENRE")
    contains("mismatch_incomplete", "missing", "DATE")
    contains("mismatch_incomplete", "differences", "GENRE")
    print("REAL_HOST_PHYSICAL_AUDIT_CLASSIFIER_PRECEDENCE=PASS")


if __name__ == "__main__":
    main()
