"""Offline, mutation-tested independent oracle for the native timing UI Commit.

Uses only synthetic MP3s generated in temporary storage, no foobar installer,
no third-party credentials, and no source metadata writes.
"""
from pathlib import Path
from tempfile import TemporaryDirectory

from assert_real_virtual_timing_ui_commit import verify, ORIGINAL, CHANGED
from make_isolated_timing_mp3 import main as generate


def rejects(before: Path, after: Path, payload: bytes) -> None:
    after.write_bytes(payload)
    try:
        verify(before, after)
    except AssertionError:
        return
    raise AssertionError("Independent timing UI oracle accepted an invalid postimage")


def main() -> None:
    with TemporaryDirectory(prefix="metadata-isolation-oracle-") as directory:
        root = Path(directory)
        before = root / "track-01.mp3"
        after = root / "postimage.mp3"
        generate(before)
        original = before.read_bytes()
        assert original.count(ORIGINAL) == 1, "fixture must have a unique target INDEX"
        expected = original.replace(ORIGINAL, CHANGED, 1)
        after.write_bytes(expected)
        verify(before, after)

        rejects(before, after, original)  # no-op
        rejects(before, after, original.replace(ORIGINAL, b"    INDEX 01 00:00:36", 1))
        rejects(before, after, expected[:-1] + bytes([expected[-1] ^ 1]))  # audio byte
        assert b"Physical Original Title" in expected
        rejects(
            before, after,
            expected.replace(b"Physical Original Title", b"Physical Original TiTle", 1),
        )
        assert b"REM UNIQUE_KEEP exactly-preserved" in expected
        rejects(
            before, after,
            expected.replace(
                b"REM UNIQUE_KEEP exactly-preserved",
                b"REM UNIQUE_KEEP exactly-preservEd", 1,
            ),
        )
    print("TIMING_REAL_UI_COMMIT_ORACLE_MUTATIONS=PASS (valid + five invalid)")


if __name__ == "__main__":
    main()
