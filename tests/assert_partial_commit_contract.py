"""Static safety gates for the three-way virtual diff policy."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DIFF = (ROOT / "src/foo_metadata_isolation/metadata_diff.cpp").read_text(encoding="utf-8")
SESSION = (ROOT / "src/foo_metadata_isolation/virtual_edit_session.cpp").read_text(
    encoding="utf-8"
)
PREFERENCES = (ROOT / "src/foo_metadata_isolation/preferences.cpp").read_text(
    encoding="utf-8"
)
RESOURCE = (ROOT / "src/foo_metadata_isolation/component.rc").read_text(encoding="utf-8")


assert 'return change.name == "STYLE"' in DIFF
assert 'change.name == "TRACKNUMBER"' in DIFF
assert "change.kind == metadata_change_kind::change" in DIFF
for forbidden in ("CUSTOM_FIELD", "MI_UNSUPPORTED_CUSTOM", "MUSICBRAINZ_TRACKID"):
    assert forbidden not in DIFF.split("bool is_known_skippable_field", 1)[1].split(
        "virtual_diff_class classify_diff_class", 1
    )[0]

assert "enum class virtual_diff_class" in (
    ROOT / "src/foo_metadata_isolation/metadata_diff.h"
).read_text(encoding="utf-8")
for classification in ("COMMITTABLE", "KNOWN_SKIPPABLE", "BLOCKING"):
    assert classification in DIFF

assert "file_info_impl expected(shadow.before);" in SESSION
assert "change.diff_class == virtual_diff_class::committable" in SESSION
assert "skip KNOWN_UNREPRESENTABLE" in SESSION
assert "reject STRICT_ALL_OR_NOTHING" in SESSION
assert "validate_virtual_results(document.path.c_str(), index, abort);" in SESSION
assert "COMMITTABLE_WITH_SKIPPED_CHANGES" in SESSION
assert "The skipped fields were left unchanged." in SESSION
assert "m_timing_prepared ?" in SESSION
assert "&planned->second : &document.virtual_before.at(subsong)" in SESSION

assert "cfg_apply_supported_changes" in PREFERENCES
assert "cfg_apply_supported_changes(guids::cfg_apply_supported_changes, true)" in PREFERENCES
assert "IDC_APPLY_SUPPORTED_CHANGES" in RESOURCE
assert "IDC_STRICT_ALL_OR_NOTHING" in RESOURCE

print("Safe partial-commit static contract assertions passed.")
