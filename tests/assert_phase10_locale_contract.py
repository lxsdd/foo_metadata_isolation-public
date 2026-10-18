"""Focused static locale-independence contract for Phase 10 product paths."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "foo_metadata_isolation"
PRODUCT = "\n".join(
    path.read_text(encoding="utf-8")
    for path in SRC.glob("*.cpp")
    if path.name != "diagnostics.cpp"
)

for forbidden in (
    "CP_ACP",
    "GetACP(",
    "setlocale(",
    "mbstowcs(",
    "wcstombs(",
    "CreateDirectoryA(",
    "FindFirstFileA(",
):
    assert forbidden not in PRODUCT, forbidden

id3 = (SRC / "id3_cuesheet_patch.cpp").read_text(encoding="utf-8")
flac = (SRC / "flac_cuesheet_patch.cpp").read_text(encoding="utf-8")
session = (SRC / "virtual_edit_session.cpp").read_text(encoding="utf-8")
cue = (SRC / "cue_patch.cpp").read_text(encoding="utf-8")
metadata = (SRC / "metadata_diff.cpp").read_text(encoding="utf-8")

assert "CP_UTF8" in id3 and "MB_ERR_INVALID_CHARS" in id3
assert "CP_UTF8" in id3 and "WC_ERR_INVALID_CHARS" in id3
assert "CP_UTF8" in flac and "MB_ERR_INVALID_CHARS" in flac
assert "CreateDirectoryW" in session and "FindFirstFileW" in session

# The locale-sensitive narrow case conversions are confined to ASCII protocol
# domains: Cue command characters and foobar metadata field identifiers.
assert "bool is_command_char(char value)" in cue
assert "(value >= 'A' && value <= 'Z')" in cue
assert "canonicalize(info.meta_enum_name(field))" in metadata
assert 'name == "REPLAYGAIN_TRACK_GAIN"' in metadata

# Timing stays integer/CD-frame based. ReplayGain runtime coverage exercises the
# SDK numeric parser/formatter; no locale-dependent decimal parser is introduced.
assert '"%02u:%02u:%02u"' in cue
assert "std::stoull" in cue or "std::stoul" in cue
assert "std::stod" not in PRODUCT and "std::stof" not in PRODUCT

print("Phase 10 locale, Unicode conversion, path and numeric formatting contract: PASS")
