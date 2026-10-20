"""Independent real-host physical TITLE writer oracle for embedded-Cue MP3.

A Physical metadata edit must update exactly that physical semantic field,
and preserve virtual Cue bytes, unrelated physical frames, opaque PRIV and MPEG.
"""
import argparse
import re
from pathlib import Path
from assert_isolated_virtual_write import parse

def decode_title(payload):
    assert payload, "empty physical TITLE"
    codec = {0:"latin-1",1:"utf-16",2:"utf-16-be",3:"utf-8"}.get(payload[0])
    assert codec, f"unsupported ID3 TIT2 encoding: {payload[0]}"
    return payload[1:].decode(codec).rstrip("\0")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("before",type=Path)
    p.add_argument("after",type=Path)
    p.add_argument("report",type=Path)
    a=p.parse_args()
    report=a.report.read_text(encoding="utf-8-sig")
    assert re.search(r"(?m)^write_result=physical_committed\s*$",report), "physical writer did not commit"
    assert "physical_title_before=Physical Original Title" in report, "unexpected physical input state"
    original,cue_before,audio_before=parse(a.before)
    changed,cue_after,audio_after=parse(a.after)
    assert decode_title(original["TIT2"][0])=="Physical Original Title"
    assert decode_title(changed["TIT2"][0])=="Physical Mix Edited", "physical TITLE not actually changed"
    for frame in ("TPE1","TALB","TCON","PRIV"):
        assert original[frame]==changed[frame],f"unrelated physical frame modified: {frame}"
    assert cue_before==cue_after, "physical writer modified raw Embedded Cue"
    assert audio_before==audio_after, "physical writer modified MPEG data"
    print("REAL_HOST_PHYSICAL_TITLE_WRITE=PASS; physical TITLE changed, all virtual Cue/audio/opaque data unchanged")

if __name__=="__main__":
    main()
