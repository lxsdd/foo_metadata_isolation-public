"""Independent raw-MP3 oracle for successful and safely rejected Cue timing changes."""
import argparse
from pathlib import Path
import re
from assert_isolated_virtual_write import parse

def main():
    p=argparse.ArgumentParser()
    p.add_argument("before",type=Path)
    p.add_argument("after",type=Path)
    p.add_argument("report",type=Path)
    p.add_argument("--scenario",choices=["index01-change","index01-invalid"],required=True)
    a=p.parse_args()
    report=a.report.read_text(encoding="utf-8-sig")
    old_frames,old_cue,old_audio=parse(a.before)
    new_frames,new_cue,new_audio=parse(a.after)
    assert old_frames==new_frames,"physical ID3 or PRIV changed"
    assert old_audio==new_audio,"MPEG changed"
    assert re.search(r"(?m)^production_timing_document_count=1\s*$",report),"not one Cue document"
    if a.scenario=="index01-change":
        assert re.search(r"(?m)^write_result=committed\s*$",report),"valid INDEX write not committed"
        old=b"    INDEX 01 00:30:00"
        new=b"    INDEX 01 00:30:35"
        assert old_cue.count(old)==1,"missing unique old INDEX"
        assert new_cue==old_cue.replace(old,new),"timing diff modified unrelated Cue bytes"
        assert old_cue!=new_cue,"no-op timing write"
    else:
        assert re.search(r"(?m)^write_result=not_committed\s*$",report),"invalid timing accepted"
        assert a.before.read_bytes()==a.after.read_bytes(),"invalid timing changed physical source"
        assert new_cue==old_cue
    print("REAL_HOST_TIMING_"+a.scenario.upper().replace("-","_")+"=PASS")

if __name__=="__main__":
    main()
