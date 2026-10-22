"""Static safety and UI contract for the v1 Physical Metadata Audit."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src/foo_metadata_isolation"

MODEL = (SRC / "physical_metadata_audit.cpp").read_text(encoding="utf-8")
MODEL_H = (SRC / "physical_metadata_audit.h").read_text(encoding="utf-8")
UI = (SRC / "physical_metadata_audit_ui.cpp").read_text(encoding="utf-8")
DIAGNOSTICS = (SRC / "diagnostics.cpp").read_text(encoding="utf-8")
CONTEXT = (SRC / "context_menu.cpp").read_text(encoding="utf-8")
PROJECT = (SRC / "foo_metadata_isolation.vcxproj").read_text(encoding="utf-8")
RESOURCE = (SRC / "component.rc").read_text(encoding="utf-8")
AGENTS = (ROOT / "AGENTS.md").read_text(encoding="utf-8")

# The scan path is read-only. The model may open readers, never writers.
assert "g_open_for_info_read" in MODEL
for forbidden in (
    "g_open_for_info_write",
    "update_info_async",
    "set_info(",
    "write_raw_",
    "commit(",
    "filesystem::g_copy",
    "filesystem::g_remove",
):
    assert forbidden not in MODEL

# v1 audit scope is the already-qualified textual Cue carriers only.
for extension in ('"mp3"', '"flac"', '"wv"'):
    assert extension in MODEL
assert '"ogg"' not in MODEL and '"opus"' not in MODEL

# Core completeness is objective and deliberately avoids TITLE/TRACKNUMBER.
for field in ('"ARTIST"', '"ALBUM ARTIST"', '"ALBUM"', '"DATE"', '"GENRE"'):
    assert field in MODEL
assert 'read_field(physical, "TITLE")' not in MODEL
assert 'read_field(physical, "TRACKNUMBER")' not in MODEL
assert "artist_group_present = album_artist.present || artist.present" in MODEL

# Comparisons reuse qualified global Cue semantics and never apply changes.
assert "global_cue_command_for_field" in MODEL
assert "cue_field_occurrence_count" in MODEL
assert "cue_field_value" in MODEL
for status in ("OK", "EMPTY", "INCOMPLETE", "MISMATCH", "REVIEW"):
    assert f'return "{status}"' in MODEL

# Whole-library and selected-source entry points share one modeless UI.
assert "show_physical_metadata_audit_library" in UI
assert "show_physical_metadata_audit_selection" in UI
assert '"Audit physical metadata for selection..."' in CONTEXT
assert '"Audit physical metadata..."' in UI
assert "threaded_process::g_run_modeless" in UI
assert "threaded_process::flag_silent" in UI
assert "WM_AUDIT_PROGRESS" in UI
assert "IDC_AUDIT_PROGRESS" in UI
assert "PBM_SETMARQUEE" in UI
assert "LVS_EX_INFOTIP" in UI
assert "m_tooltips.add_control(" in UI
assert "ListView_GetHeader(m_list)" in UI
assert "modeless_dialog_manager::g_add(window)" in UI
assert "modeless_dialog_manager::g_remove(window)" in UI
assert "case IDCANCEL:" in UI
assert "shutdown_physical_metadata_audit" in UI
assert '"FOOBAR_SHUTDOWN"' in UI
assert "shutdown_physical_metadata_audit();" in (SRC / "component.cpp").read_text(encoding="utf-8")
assert "m_pending_scan" in UI
assert "std::set<std::string, audit_path_less> candidate_paths" in UI
assert "candidate_paths.emplace" in UI
assert '"Problems"' in UI
assert "LVS_EX_HEADERDRAGDROP" in UI
assert "ListView_SetColumnOrderArray" in UI
assert "ListView_GetSelectedCount(m_list) == 0" in UI
assert "cfg_string g_cfg_audit_columns" in UI
assert "WM_DPICHANGED" in UI
assert "m_dark.AddDialogWithControls" in UI
assert "physical_audit_tooltip_definitions" in UI
# Native ListView rows may be unavailable through hosted UIA. Accessible
# fallback is a production read-only EDIT mirroring the actual selected row.
assert "IDC_AUDIT_ACCESSIBLE_DETAIL" in RESOURCE
assert "ES_READONLY | ES_AUTOHSCROLL | WS_TABSTOP" in RESOURCE
assert "update_accessible_result_detail()" in UI
assert "EVENT_OBJECT_VALUECHANGE" in UI
assert "PRODUCTION_AUDIT_SELECTED_RESULT_READONLY_UIA_VALUE=PASS" in (
    ROOT / "tests/assert_real_audit_uia.ps1").read_text(encoding="utf-8")

# The only writer-capable transition is an explicit user action to existing
# Physical Properties; the scan/analysis path itself remains read-only.
assert "standard_commands::context_file_properties" in UI
assert '"Edit physical metadata..."' in RESOURCE
assert '"read_only"' in UI

# Deterministic diagnostic builds exercise the classifier without writing the source.
assert "/metadata-isolation-probe-physical-audit-classifier" in DIAGNOSTICS
assert "assess_physical_metadata" in DIAGNOSTICS
assert "write_result=read_only_classifier" in DIAGNOSTICS
assert (ROOT / "tests/assert_physical_metadata_audit_runtime.py").exists()
assert (ROOT / "tests/run_physical_metadata_audit_runtime.ps1").exists()

# Production build wiring and centralized governance are explicit.
for source in ("physical_metadata_audit.cpp", "physical_metadata_audit_ui.cpp"):
    assert source in PROJECT
for standard in (
    "GITHUB-FIRST.md",
    "RELEASE-POLICY.md",
    "FOOBAR2000-COMPONENT.md",
    "SHARED-DESKTOP-UX.md",
    "UI-RUNTIME-QUALIFICATION.md",
):
    assert standard in AGENTS

print("Physical Metadata Audit static safety/UI contract: PASS")
