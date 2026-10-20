"""Independent raw ID3v2.4 + MPEG preservation oracle for virtual-Cue writes.

No green result from process exit alone: check report, exact physical frame
payloads, exact audio bytes, and exact unrelated raw CUE directives.
"""
import argparse
from pathlib import Path
import re

def ss(raw):
    assert len(raw) == 4 and all(v < 128 for v in raw), "invalid synchsafe"
    return raw[0] << 21 | raw[1] << 14 | raw[2] << 7 | raw[3]

def parse(path):
    data = Path(path).read_bytes()
    assert data.startswith(b"ID3\x04\x00\x00"), "unexpected ID3 version/flags"
    end = 10 + ss(data[6:10])
    assert end < len(data)
    frames, pos = {}, 10
    while pos + 10 <= end and data[pos:pos+4] != bytes(4):
        name = data[pos:pos+4].decode("ascii")
        assert re.fullmatch(r"[A-Z0-9]{4}", name), name
        count = ss(data[pos+4:pos+8])
        assert data[pos+8:pos+10] == b"\0\0", "unexpected frame flags"
        payload = data[pos+10:pos+10+count]
        assert len(payload) == count
        frames.setdefault(name, []).append(payload)
        pos += count + 10
    assert set(frames) == {"TIT2","TPE1","TALB","TCON","PRIV","TXXX"}, set(frames)
    assert len(frames["TXXX"]) == 1
    value = frames.pop("TXXX")[0]
    assert value.startswith(b"\x03CUESHEET\0"), "missing UTF-8 embedded Cue"
    return frames, value[len(b"\x03CUESHEET\0"):], data[end:]

def without_virtual_title_values(cue):
    return re.sub(rb'(?m)^([ \t]+TITLE ")[^"\r\n]*(")',rb'\1<VIRTUAL_TITLE>\2', cue)

def main():
    cli = argparse.ArgumentParser()
    cli.add_argument("before", type=Path)
    cli.add_argument("after", type=Path)
    cli.add_argument("report", type=Path)
    cli.add_argument("--scenario", choices=("title","partial-style","strict","blocking","rollback"), default="title")
    args = cli.parse_args()
    report = args.report.read_text(encoding="utf-8-sig")
    physical_before, cue_before, audio_before = parse(args.before)
    physical_after, cue_after, audio_after = parse(args.after)
    assert physical_before == physical_after, "physical ID3 fields or opaque PRIV changed"
    assert audio_before == audio_after, "MPEG payload changed"
    if args.scenario in ("strict", "blocking", "rollback"):
        assert re.search(r"(?m)^write_result=not_committed\\s*$", report), "rejected writer reported a commit"
        assert args.before.read_bytes() == args.after.read_bytes(), "rejected/rolled-back transaction changed source bytes"
        print("VIRTUAL_"+args.scenario.upper()+"_REAL_FOOBAR_HOST=PASS; original byte-identical")
        return
    assert re.search(r"(?m)^write_result=committed\\s*$", report), "probe did not commit"
    assert re.search(r"(?m)^production_virtual_track_count=2\\s*$", report), "wrong virtual count"
    assert cue_before != cue_after, "Cue was not updated"
    assert b'TITLE "Virtual Original One"' in cue_before
    assert b'TITLE "Virtual Original Two"' in cue_before
    assert without_virtual_title_values(cue_before) == without_virtual_title_values(cue_after), "unrelated raw Cue changed"
    assert (b'TITLE "Virtual Original One"' not in cue_after or
            b'TITLE "Virtual Original Two"' not in cue_after), "no virtual title change"
    assert b'REM UNIQUE_KEEP exactly-preserved' in cue_after
    if args.scenario == "partial-style":
        assert "skip KNOWN_UNREPRESENTABLE" in report, "missing explicit STYLE skip evidence"
        assert b"STYLE" not in cue_after and b"STYLE" not in cue_before, "STYLE was unexpectedly serialized"
    print("VIRTUAL_"+args.scenario.upper()+"_REAL_FOOBAR_HOST=PASS; physical frames, opaque PRIV and audio exact")

if __name__ == "__main__":
    main()
