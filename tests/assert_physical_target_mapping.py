"""Independent fail-closed oracle for the real foobar physical-target mapper.

The actual C++ map_to_physical_targets() runs in a disposable Win32/x64
foobar2000 host. This verifier only reads its report and synthetic fixture,
never writes music files or fabricates a mapped target.
"""
import argparse
import hashlib
from pathlib import Path


CASES = {
    "single": {"extra": 0, "target": 1, "rejected": 0, "complete": "true"},
    "duplicate": {"extra": 1, "target": 1, "rejected": 0, "complete": "true"},
    "all-subsongs": {"extra": 0, "target": 1, "rejected": 0, "complete": "true"},
    "remote-rejected": {"extra": 1, "target": 1, "rejected": 1, "complete": "false"},
    "many-distinct": {"extra": 0, "target": 111, "rejected": 0, "complete": "true"},
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
    # foobar's real command-line parser may expand a Cue file argument into
    # multiple virtual handles. Derive the expected input cardinality from
    # that independently emitted launcher count; never assume it is one.
    source_handles = int(data.get("handle_count", "0"))
    require(1 <= source_handles <= 3, "Unexpected original Cue launcher handle count")
    expected_inputs = (224 if options.scenario == "many-distinct"
                       else source_handles * 3 if options.scenario == "all-subsongs"
                       else source_handles + expected["extra"])
    for key, value in (
        ("mapping_input_count", expected_inputs),
        ("mapping_target_count", expected["target"]),
        ("mapping_rejected_count", expected["rejected"]),
        ("mapping_complete", expected["complete"]),
        ("mapping_target_0_subsong", "0"),
        ("write_result", "map_only"),
    ):
        require(data.get(key) == str(value), f"{options.scenario}: wrong {key}")
    if options.scenario == "many-distinct":
        # The real SDK mapper returns the 111 unique source handles in the
        # selection's original descending order, despite using a sorted set
        # internally for de-duplication.
        require(len(list(fixture.parent.glob("track-*.mp3"))) == 111,
                "Expected exactly 111 distinct disposable MP3 carriers")
        baseline_digest = digest(baseline)
        for index in range(111):
            number = 111 - index
            leaf = f"track-{number:02d}.mp3"
            mapped = data.get(f"mapping_target_{index}_path", "")
            require(mapped.lower().endswith(leaf), f"Physical selection order changed at {index}")
            require(data.get(f"mapping_target_{index}_subsong") == "0",
                    f"Virtual subsong leaked into physical target {index}")
            require(digest(fixture.parent / leaf) == baseline_digest,
                    f"Synthetic source {number} was altered")
        require("mapping_target_111_path" not in data, "Additional duplicate source leaked")
    else:
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

    print("REAL_HOST_PHYSICAL_TARGET_MAPPING_" +
          options.scenario.upper().replace("-", "_") + "=PASS")


if __name__ == "__main__":
    main()
