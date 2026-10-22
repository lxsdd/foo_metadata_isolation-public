"""Deterministic contract gates for Phase 12 Physical metadata breadth."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAPPER = (ROOT / "src/foo_metadata_isolation/physical_targets.cpp").read_text(
    encoding="utf-8"
)
CONTEXT = (ROOT / "src/foo_metadata_isolation/context_menu.cpp").read_text(
    encoding="utf-8"
)
DIAGNOSTICS = (ROOT / "src/foo_metadata_isolation/diagnostics.cpp").read_text(
    encoding="utf-8"
)
PROJECT = (ROOT / "src/foo_metadata_isolation/foo_metadata_isolation.vcxproj").read_text(
    encoding="utf-8"
)
RUNTIME = (ROOT / "tests/run_phase12_physical_runtime.ps1").read_text(
    encoding="utf-8"
)
ISOLATED_HOST_RUNTIME = (ROOT / "tests/run_isolated_virtual_write.ps1").read_text(
    encoding="utf-8"
)
FIXTURES = (ROOT / "tests/make_phase12_physical_fixtures.py").read_text(
    encoding="utf-8"
)
ORACLE = (ROOT / "tests/assert_phase12_physical_oracle.py").read_text(
    encoding="utf-8"
)


# Physical routing canonicalizes local sources, rejects non-local sources,
# deduplicates by canonical source with the original filesystem-aware comparison,
# and always creates one physical subsong-0 handle. The prior per-item scan of
# all already-created handles was quadratic for mostly distinct selections.
for marker in (
    "filesystem::g_get_canonical_path",
    "source_filesystem->is_remote",
    "filesystem::g_get_native_path",
    "std::set<std::string, physical_source_less>",
    "filesystem::g_compare_paths(left.c_str(), right.c_str(), comparison)",
    "metadb::path_compare(left.c_str(), right.c_str()) < 0",
    "unique_physical_sources.find(key)",
    "handle_create(canonical_path, 0)",
    "unique_physical_sources.insert(key)",
):
    assert marker in MAPPER
assert "for (const auto& existing : result.handles)" not in MAPPER
# The canonicalized filesystem-aware comparator must collapse distinct-case
# names for one local Windows file in the original host, not only in a mock.
assert "/metadata-isolation-probe-map-physical-case-alias" in DIAGNOSTICS
assert "map_physical_with_case_alias" in DIAGNOSTICS
assert "scenario='case-alias'" in ISOLATED_HOST_RUNTIME
assert '/metadata-isolation-probe-map-physical-remote-first' in DIAGNOSTICS
# The remote-first scenario is launched from the original isolated Win32/x64
# host script, not the separate Phase-12 six-source format matrix.
assert "remote-first-rejected" in ISOLATED_HOST_RUNTIME
assert "/metadata-isolation-probe-map-physical-remote-first" in ISOLATED_HOST_RUNTIME
assert "assert_physical_target_mapping.py" in ISOLATED_HOST_RUNTIME
assert MAPPER.index("handle_create(canonical_path, 0)") < MAPPER.index(
    "unique_physical_sources.insert(key)"
)

# A partially unmappable selection never reaches the delegated command.
assert "!mapping.complete() || mapping.empty()" in CONTEXT
assert "No operation was started." in MAPPER

# The broad matrix is development-only. Normal builds omit diagnostics.cpp.
assert "EnableTestProbes" in PROJECT
assert "diagnostics.cpp" in PROJECT
assert "/metadata-isolation-probe-physical-matrix" in DIAGNOSTICS
assert "/metadata-isolation-probe-physical-replaygain-remove" in DIAGNOSTICS

# Physical fields are handed to the native writer as real file_info state;
# custom and MusicBrainz names are not passed through the Virtual classifier.
for marker in (
    'info.meta_set("TITLE", "Physical Matrix Title',
    'info.meta_set("DISCNUMBER", "2")',
    'info.meta_set("CUSTOM_FIELD", "changed',
    '"MUSICBRAINZ_RELEASEID",',
    'info.meta_set("PHASE12_VECTOR",',
    'info.meta_remove_field("MULTI_VALUE_FIELD")',
    'info.meta_remove_field("COMMENT")',
    "info.set_replaygain(rg)",
    "input_entry::g_open_for_info_write_timeout",
):
    assert marker in DIAGNOSTICS

# Runtime coverage is serial per architecture and uses the six-source mixed
# MP3/FLAC corpus plus independent raw-container validation.
for marker in (
    '("id3v2", False, False)',
    '("id3v2-id3v1", False, True)',
    '("id3v2-apev2", True, False)',
    '("id3v2-apev2-id3v1", True, True)',
    "physical-textual.flac",
    "physical-native.flac",
):
    assert marker in FIXTURES
for marker in (
    "assert_phase12_physical_oracle.py",
    "Expected six Physical sources",
    "/metadata-isolation-probe-map-all-handles-physical",
):
    assert marker in RUNTIME

# The oracle locks the preservation boundary at actual container bytes rather
# than relying on UI output or formatted metadata strings.
for marker in (
    'before["audio_payload"]["sha256"] == after["audio_payload"]["sha256"]',
    'before["id3v1"] == after["id3v1"]',
    'before["apev2"] == after["apev2"]',
    'before["block_bytes"][index] == after["block_bytes"][index]',
    'before["pictures"] == after["pictures"]',
    'before["cuesheet"]["text"] == after["cuesheet"]["text"]',
    "MUSICBRAINZ_RELEASEID",
    "PHASE12_VECTOR",
    'choices=("matrix", "remove")',
):
    assert marker in ORACLE

print("Phase 12 Physical routing and format/container contract: PASS")
