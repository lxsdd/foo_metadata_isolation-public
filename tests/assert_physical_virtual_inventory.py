"""Independent original-host physical/virtual Cue read-only inventory oracle."""
import argparse
from pathlib import Path


def check(report):
    data = {}
    for line in report.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        assert key not in data, f"Duplicate report key: {key}"
        data[key] = value.strip()

    def title(prefix):
        found = []
        for i in range(int(data[f"{prefix}_field_count"])):
            name = bytes.fromhex(data[f"{prefix}_field_{i}_name_hex"]).decode("utf-8")
            if name.casefold() == "title":
                assert data[f"{prefix}_field_{i}_value_count"] == "1"
                found.append(bytes.fromhex(data[f"{prefix}_field_{i}_value_0_hex"]).decode("utf-8"))
        assert len(found) == 1, f"Expected exactly one TITLE for {prefix}"
        return found[0]

    assert data["inventory_subsong_count"] == "2"
    assert data["physical_reader_target_subsong"] == "0"
    assert int(data["physical_reader_subsongs"]) >= 1
    assert data["inventory_subsong_0"] == "1"
    assert data["inventory_subsong_1"] == "2"
    assert title("physical_track_0") == "Physical Original Title"
    assert title("inventory_track_1") == "Virtual Original One"
    assert title("inventory_track_2") == "Virtual Original Two"
    assert data["inventory_protection_after"] == "UNKNOWN"
    assert data["write_result"] == "inventory_only"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    check(args.report.read_text(encoding="utf-8-sig"))
    print("REAL_HOST_PHYSICAL_0_VIRTUAL_CUE_READONLY_PREVIEW=PASS")
