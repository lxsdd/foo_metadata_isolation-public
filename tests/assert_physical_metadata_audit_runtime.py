"""Runtime oracle for the deterministic Physical Metadata Audit classifier probe."""

from __future__ import annotations

import argparse
from pathlib import Path


def report_for(source: Path) -> Path:
    return source.with_name(source.name + ".metadata-isolation-probe.txt")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    text = report_for(args.source.resolve()).read_text(encoding="utf-8-sig")

    expected = {
        "ok_status": "OK",
        "artist_fallback_status": "OK",
        "empty_status": "EMPTY",
        "incomplete_status": "INCOMPLETE",
        "whitespace_status": "INCOMPLETE",
        "mismatch_status": "MISMATCH",
        "review_status": "REVIEW",
        "write_result": "read_only_classifier",
    }
    values = dict(
        line.split("=", 1)
        for line in text.splitlines()
        if "=" in line and not line.startswith("structured_log=")
    )
    for key, value in expected.items():
        assert values.get(key) == value, (key, values.get(key), value)

    assert values["ok_missing"] == ""
    assert values["artist_fallback_missing"] == ""
    assert values["empty_missing"] == "ARTIST / ALBUM ARTIST, ALBUM, DATE, GENRE"
    assert values["incomplete_missing"] == "GENRE"
    assert values["whitespace_missing"] == "DATE"
    assert values["mismatch_differences"] == "GENRE"
    assert values["review_review"] == "GENRE"

    # The synthetic complete case intentionally contains neither physical TITLE
    # nor TRACKNUMBER; OK proves they are not source-file completeness gates.
    assert values["ok_status"] == "OK"

    print("Physical Metadata Audit deterministic classifier runtime: PASS")


if __name__ == "__main__":
    main()
