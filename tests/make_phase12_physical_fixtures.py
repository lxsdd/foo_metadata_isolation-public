"""Create the small physical MP3/FLAC breadth matrix used by Phase 12."""

from __future__ import annotations

import argparse
from pathlib import Path

from make_fixture import add_apev2, add_id3v1, create_fixture
from make_flac_cuesheet_fixtures import add_native_cuesheet_block, create_base


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = args.output.resolve()
    if root.exists():
        raise SystemExit(f"Refusing existing output directory: {root}")
    root.mkdir(parents=True)
    fixtures = root / "Unicode-東京-Δ"
    fixtures.mkdir()

    for name, ape, id3v1 in (
        ("id3v2", False, False),
        ("id3v2-id3v1", False, True),
        ("id3v2-apev2", True, False),
        ("id3v2-apev2-id3v1", True, True),
    ):
        path = fixtures / f"physical-{name}.mp3"
        create_fixture(path, advanced_cue=True)
        if ape:
            add_apev2(path)
        if id3v1:
            add_id3v1(path)

    create_base(fixtures / "physical-textual.flac")
    # The native carrier is intentionally read-only evidence: it establishes
    # whether a host physical writer preserves a competing type-5 block.
    create_base(fixtures / "physical-native.flac")
    add_native_cuesheet_block(fixtures / "physical-native.flac")
    print(root)


if __name__ == "__main__":
    main()
