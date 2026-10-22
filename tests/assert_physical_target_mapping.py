"""Independent fail-closed oracle for the real foobar physical-target mapper.

The actual C++ map_to_physical_targets() runs in a disposable Win32/x64
foobar2000 host. This verifier only reads its report and synthetic fixture,
never writes music files or fabricates a mapped target.
"""
import argparse
import hashlib
from pathlib import Path


CASES = {
    "single": {"input": 1, "target": 1, "rejected": 0, "complete": "true"},
    "duplicate": {"input": 2, "target": 1, "rejected": 0, "complete": "true"},
    "all-subsongs": {"input": 3, "target": 1, "rejected": 0, "complete": "true"},
    "remote-rejected": {"input": 2, "target": 1, "rejected": 1, "complete": "false"},
}


def require(condition: bool, label: str) -> None:
    if not condition:
        raise AssertionError(label)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--scenario", choices=sorted(CASES), required=True)
    options = parser.parse_args()
    report = options.report
    fixture = options.fixture
    baseline = options.baseline
    require(fixture.name == "track-01.mp3", "Unexpected media fixture")
    require(baseline.name == "track-01.mp3.before", "Unexpected baseline")
    require(report.name == fixture.name + ".metadata-isolation-probe.txt", "Unexpected report")
    require(report.parent == fixture.parent == baseline.parent, "Cross-directory report refused")
    require(digest(fixture) == digest(baseline), "Read-only physical mapping mutated MP3")

    data = {}
    for line in report.read_text(encoding="utf-8-sig").splitlines():
        if not line:
            continue
        require("=" in line, "Malformed mapping report")
        key, value = line.split("=", 1)
        require(key not in data, "Duplicate mapping evidence")
        data[key] = value

    expected = CASES[options.scenario]
    for key, value in (
        ("mapping_input_count", expected["input"]),
        ("mapping_target_count", expected["target"]),
        ("mapping_rejected_count", expected["rejected"]),
        ("mapping_complete", expected["complete"]),
        ("mapping_target_0_subsong", "0"),
        ("write_result", "map_only"),
    ):
        require(data.get(key) == str(value), f"{options.scenario}: wrong {key}")
    path = data.get("mapping_target_0_path", "")
    require(path.lower().endswith("track-01.mp3"), "Unexpected physical target path")
    require("mapping_target_1_path" not in data, "Duplicated physical target")

    if options.scenario == "remote-rejected":
        rejected = data.get("mapping_first_rejected_path", "")
        require(rejected.startswith("http://127.0.0.1/metadata-isolation-unsupported.mp3"),
                "Missing known unsupported remote selection")
        require(bool(data.get("mapping_first_rejected_reason")), "Missing rejection reason")
    else:
        require("mapping_first_rejected_path" not in data, "Unexpected rejected source")

    if options.scenario == "all-subsongs":
        require(data.get("handle_count") == "1", "Wrong launcher fixture count")

    print("REAL_HOST_PHYSICAL_TARGET_MAPPING_" +
          options.scenario.upper().replace("-", "_") + "=PASS")


if __name__ == "__main__":
    main()
