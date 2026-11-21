"""Independent oracle for the real Masstagger 1.9.3 physical COMMENT write.

Decodes raw ID3v2.3/v2.4, preserving every independent physical tag and
opaque PRIV payload. MPEG bytes must be byte-identical. Cannot use the
virtual-Cue oracle: this is intentionally a *plain* no-CUE audio file.
"""
import argparse
import re
from pathlib import Path


def syncsafe(raw: bytes) -> int:
    assert len(raw) == 4 and all(c < 128 for c in raw)
    return (raw[0] << 21) | (raw[1] << 14) | (raw[2] << 7) | raw[3]


def parse_plain(path: Path):
    data=path.read_bytes()
    assert data[:3] == b"ID3" and data[3] in (3,4) and data[4] == 0, "Expected real MP3 ID3v2.3/v2.4"
    assert data[5] & 0x3F == 0, "Unexpected ID3 header flags"
    end=10+syncsafe(data[6:10])
    assert 10 < end < len(data) and end < 1_000_000
    frames, cursor={},10
    while cursor+10 <= end and data[cursor:cursor+4] != b"\0"*4:
        name=data[cursor:cursor+4].decode("ascii")
        assert re.fullmatch("[A-Z0-9]{4}",name), "Unknown ID3 frame identifier"
        size=syncsafe(data[cursor+4:cursor+8]) if data[3]==4 else int.from_bytes(data[cursor+4:cursor+8],"big")
        assert data[cursor+8:cursor+10] == b"\0\0", "Unexpected ID3 frame flags"
        assert size > 0 and cursor+10+size <= end
        frames.setdefault(name,[]).append(data[cursor+10:cursor+10+size])
        cursor+=10+size
    return frames,data[end:]


def decode_comm(p: bytes) -> str:
    assert len(p) > 5
    encoding={0:"latin-1",1:"utf-16",2:"utf-16-be",3:"utf-8"}.get(p[0])
    assert encoding, "Unknown comment encoding"
    # COMM encoding: [encoding][3-char language][description terminated][comment]
    if encoding in ("utf-16","utf-16-be"):
        desc_terminator=p.find(b"\0\0",4)
        assert desc_terminator >= 4
        content=p[desc_terminator+2:]
    else:
        terminator=p.find(b"\0",4)
        assert terminator >= 4
        content=p[terminator+1:]
    return content.decode(encoding).strip("\0")


def main(before:Path,after:Path,token:str):
    if token != "MI_SYNTHETIC_MASSTAGGER_193":
        raise AssertionError("Unexpected disposable test token")
    pre,audio_pre=parse_plain(before)
    post,audio_post=parse_plain(after)
    assert set(pre) == {"TIT2","TPE1","TALB","TCON","PRIV"}
    assert "TXXX" not in post, "Masstagger unexpectedly created an embedded Cue"
    assert audio_pre==audio_post, "Real Masstagger changed MPEG bytes"
    assert pre["PRIV"]==post.get("PRIV"), "Opaque PRIV data was lost"
    for field in ("TIT2","TPE1","TALB","TCON"):
        assert pre[field]==post.get(field), f"Unrelated ID3 physical frame modified: {field}"
    assert set(post)==set(pre)|{"COMM"}, f"Real Masstagger altered unrelated frame set: {set(post)}"
    assert len(post["COMM"])==1
    assert decode_comm(post["COMM"][0])==token, "Actual Masstagger CLI did not write the intended comment"
    print("MASSTAGGER_PLAIN_MP3_PHYSICAL_TAG_COMMIT_INDEPENDENT_ORACLE=PASS")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("before",type=Path)
    p.add_argument("after",type=Path)
    p.add_argument("token")
    a=p.parse_args()
    main(a.before,a.after,a.token)
