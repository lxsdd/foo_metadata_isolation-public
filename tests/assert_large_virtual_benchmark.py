"""Independent 111-track virtual projection / commit oracle.

Test requires actual host PASS markers AND verifies raw CUE changes, while all
non-TITLE Cue tokens, physical ID3/PRIV frames and MPEG audio stay exact.
"""
import argparse
from pathlib import Path
import re
from assert_isolated_virtual_write import parse, without_virtual_title_values

COUNT = 111

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    report = args.report.read_text(encoding="utf-8-sig")
    for key, value in (
        ("production_virtual_track_count", str(COUNT)),
        ("benchmark_selection_count", str(COUNT)),
        ("benchmark_prepared", "true"),
        ("benchmark_committed", "true"),
        ("benchmark_result", "PASS"),
        ("write_result", "benchmark_pass")
    ):
        assert re.search(r"(?m)^" + re.escape(key + "=" + value) + r"\s*$", report), f"missing: {key}={value}"
    initial_tags, initial_cue, initial_audio = parse(args.before)
    final_tags, final_cue, final_audio = parse(args.after)
    assert initial_tags == final_tags, "physical tags or PRIV changed"
    assert initial_audio == final_audio, "MPEG payload changed"
    assert without_virtual_title_values(initial_cue) == without_virtual_title_values(final_cue), "unrelated raw Cue changed"
    old_titles = re.findall(rb'(?m)^    TITLE "([^"\r\n]*)"', initial_cue)
    new_titles = re.findall(rb'(?m)^    TITLE "([^"\r\n]*)"', final_cue)
    assert len(old_titles) == len(new_titles) == COUNT, (len(old_titles), len(new_titles))
    assert all(a != b for a,b in zip(old_titles,new_titles)), "some virtual titles were unchanged"
    assert initial_cue.count(b"INDEX 01") == final_cue.count(b"INDEX 01") == COUNT
    keys = dict(re.findall(r"(?m)^(benchmark_[a-z_]+)=(\d+)$", report))
    for key in ("benchmark_total_us", "benchmark_process_read_bytes", "benchmark_process_write_bytes"):
        assert key in keys and int(keys[key]) >= 0, f"missing timing: {key}"
    print("REAL_HOST_111_VIRTUAL_COMMIT=PASS")
    for key in sorted(keys):
        if key.endswith(("_us","_bytes")):
            print(f"MI_111_{key.upper()}={keys[key]}")

if __name__ == "__main__":
    main()
