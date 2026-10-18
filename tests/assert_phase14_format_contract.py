"""Deterministic static contract for the Phase-14 carrier boundary."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src/foo_metadata_isolation"
RAW_H = (SRC / "raw_cuesheet_patch.h").read_text(encoding="utf-8")
RAW = (SRC / "raw_cuesheet_patch.cpp").read_text(encoding="utf-8")
APE = (SRC / "apev2_cuesheet_patch.cpp").read_text(encoding="utf-8")
SESSION = (SRC / "virtual_edit_session.cpp").read_text(encoding="utf-8")
GUARD = (SRC / "write_guard.cpp").read_text(encoding="utf-8")
PROJECT = (SRC / "foo_metadata_isolation.vcxproj").read_text(encoding="utf-8")
PHYSICAL = (ROOT / "docs/PHYSICAL_METADATA_CAPABILITY_MATRIX.md").read_text(encoding="utf-8")
VIRTUAL = (ROOT / "docs/VIRTUAL_METADATA_CAPABILITY_MATRIX.md").read_text(encoding="utf-8")
STATUS = (ROOT / "docs/V1_DEVELOPMENT_STATUS.md").read_text(encoding="utf-8")


assert "wavpack_apev2" in RAW_H
assert 'pfc::stricmp_ascii(extension, "wv") == 0' in RAW
assert "validate_raw_wavpack_apev2_cuesheet" in RAW
assert "write_raw_wavpack_apev2_cuesheet_patch" in RAW
assert 'pfc::stricmp_ascii(extension, "ogg")' not in RAW
assert 'pfc::stricmp_ascii(extension, "opus")' not in RAW

for boundary in (
    "exactly one CUESHEET APEv2 item",
    "not valid NUL-free UTF-8",
    "APEv2 item is read-only",
    "APEv2 item has unsupported flags",
    "Unsupported APEv2 footer flags or reserved bytes",
    "Raw WavPack CUESHEET does not match",
    "exceeds the safety limit",
):
    assert boundary in APE
assert "k_max_tag_bytes = 64u * 1024u * 1024u" in APE
assert "copy_prefix(input, output, layout.extent_start" in APE

assert SESSION.count('pfc::stricmp_ascii(selected_extension, "wv")') == 1
assert 'pfc::stricmp_ascii(extension, "wv") == 0' in SESSION
assert 'pfc::stricmp_ascii(extension, "wv") == 0' in GUARD
assert "MP3/FLAC/WavPack subsong writes" in GUARD
assert "apev2_cuesheet_patch.cpp" in PROJECT

assert "WavPack / APEv2" in PHYSICAL and "SUPPORTED THROUGH HOST WRITER" in PHYSICAL
assert "Ogg Vorbis / Vorbis Comments" in PHYSICAL
assert "Opus / OpusTags" in PHYSICAL
assert "Monkey's Audio / APEv2" in PHYSICAL and "DEFERRED POST-v1.0" in PHYSICAL
assert "MP4 chapters" in VIRTUAL and "Matroska chapters" in VIRTUAL
assert "Ogg Vorbis `CUESHEET` comment" in VIRTUAL
assert "Opus `CUESHEET` comment" in VIRTUAL
assert "| 14 | additional formats and Virtual mechanisms | PASS / closed |" in STATUS
assert "Phase 14: additional formats/mechanisms" not in STATUS

print("Phase 14 format/carrier static contract PASS")
