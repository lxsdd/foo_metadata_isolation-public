"""Independent postimage oracle for real Masstagger on an embedded-CUE MP3.

Masstagger must edit ONLY physical COMMENT (the CLI targets the physical
file), preserving original virtual CUE text, physical unknown PRIV bytes
and every MPEG byte. Genuine foobar2000 host and Masstagger 1.9.3 do the
write; this file only observes pre- and postimage bytes.
"""
import argparse
from pathlib import Path
from assert_masstagger_plain_tag_write import parse_plain,decode_comm


def check(before:Path,after:Path,expected:str):
    # Two immutable, reviewed disposable-only action values: the original
    # third-party CLI probe and the genuine Set Value -> Run UI probe.
    # Never accept arbitrary expected data from a user file or preset.
    assert expected in {"MI_SYNTHETIC_CUE_PHYSICAL_193", "MI_SYNTHETIC_MASSTAGGER_REAL_UI_193"}
    a,audio_a=parse_plain(before)
    b,audio_b=parse_plain(after)
    required={"TIT2","TPE1","TALB","TCON","PRIV","TXXX"}
    assert set(a)==required
    assert set(b)==required|{"COMM"}, f"Unexpected embedded Cue MP3 physical frames: {set(b)}"
    for frame in sorted(required):
        assert a[frame]==b[frame],f"Real Masstagger altered original physical/Cue frame: {frame}"
    original_cue=a["TXXX"][0]
    assert original_cue.startswith(b"\x03CUESHEET\0")
    assert original_cue.count(b"INDEX 01")==2
    assert b"REM UNIQUE_KEEP exactly-preserved" in original_cue
    assert audio_a==audio_b,"MPEG payload modified during third-party tagging"
    assert len(b["COMM"])==1
    assert decode_comm(b["COMM"][0])==expected, "Physical COMMENT not genuinely committed"
    print("REAL_MASSTAGGER_EMBEDDED_CUE_PHYSICAL_ID3_AND_AUDIO_ORACLE=PASS")
    print("MASSTAGGER_EMBEDDED_CUE_RAW_TXXX_AND_UNKNOWN_PRIV_BYTE_EXACT=PASS")


if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("before",type=Path)
    p.add_argument("after",type=Path)
    p.add_argument("comment")
    args=p.parse_args()
    check(args.before,args.after,args.comment)
