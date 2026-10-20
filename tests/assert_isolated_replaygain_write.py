"""Real foobar Virtual ReplayGain commit must preserve every unrelated byte.

Only REM REPLAYGAIN_* lines may be added to this initially ReplayGain-free
synthetic embedded Cue. No physical ReplayGain is written in the ID3 tags.
"""
import argparse
import re
from pathlib import Path
from assert_isolated_virtual_write import parse

RG_LINE = re.compile(rb"(?m)^[ \t]*REM REPLAYGAIN_[A-Z_]+[^\r\n]*\r?\n")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("before",type=Path)
    p.add_argument("after",type=Path)
    p.add_argument("report",type=Path)
    a=p.parse_args()
    report=a.report.read_text(encoding="utf-8-sig")
    assert re.search(r"(?m)^write_result=committed\s*$", report), "Virtual ReplayGain not committed"
    assert re.search(r"(?m)^production_virtual_track_count=2\s*$", report), "wrong virtual selection"
    original, cue_old, audio_old=parse(a.before)
    updated, cue_new, audio_new=parse(a.after)
    assert original == updated, "physical ID3/PRIV changed during virtual ReplayGain write"
    assert audio_old == audio_new, "MPEG changed"
    assert b"REPLAYGAIN_" not in cue_old
    assert b"REPLAYGAIN_TRACK_GAIN" in cue_new
    assert b"REPLAYGAIN_TRACK_PEAK" in cue_new
    assert b"REPLAYGAIN_ALBUM_GAIN" in cue_new
    assert b"REPLAYGAIN_ALBUM_PEAK" in cue_new
    raw_without_rg=RG_LINE.sub(b"",cue_new)
    assert raw_without_rg == cue_old, "unexpected CUE changes beyond ReplayGain directives"
    assert cue_new != cue_old, "virtual ReplayGain was a no-op"
    print("REAL_HOST_VIRTUAL_REPLAYGAIN=PASS; all four RG components, raw Cue unrelated text/audio/ID3 exact")

if __name__=="__main__":
    main()
