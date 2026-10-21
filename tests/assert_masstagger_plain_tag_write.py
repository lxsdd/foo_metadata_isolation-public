"""Independent oracle for actual Masstagger non-noop physical COMMENT write."""
import argparse
from pathlib import Path
from assert_isolated_virtual_write import parse
def main(before:Path,after:Path,token:str):
    if not token.startswith("MI_SYNTHETIC_MASSTAGGER_"):
        raise AssertionError("Not the disposable test token")
    pre, cue_pre, audio_pre=parse(before)
    post, cue_post, audio_post=parse(after)
    assert cue_pre==cue_post==b"", "Unexpected embedded Cue or rewritten Cue"
    assert audio_pre==audio_post, "Masstagger changed MPEG bytes"
    assert pre.get("PRIV")==post.get("PRIV"), "Opaque physical PRIV changed"
    for key in ("TIT2","TPE1","TALB","TCON"):
        assert pre.get(key)==post.get(key),f"Unrelated physical frame changed: {key}"
    assert set(post)-set(pre) <= {"COMM"}, "Masstagger added unexpected ID3 fields"
    assert "COMM" in post and post["COMM"], "Masstagger did not add physical COMM"
    content=post["COMM"][0]
    assert token.encode("ascii") in content, "Masstagger output missing actual requested comment"
    print("MASSTAGGER_PLAIN_MP3_PHYSICAL_TAG_COMMIT_INDEPENDENT_ORACLE=PASS")
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("before",type=Path)
    p.add_argument("after",type=Path)
    p.add_argument("token")
    a=p.parse_args()
    main(a.before,a.after,a.token)
