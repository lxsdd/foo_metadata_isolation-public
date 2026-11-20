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
assert "PRODUCTION_AUDIT_SELECTED_RESULT_READONLY_MSAA_VALUE=PASS" in (
    ROOT / "tests/assert_real_audit_uia.ps1").read_text(encoding="utf-8")
# A real zero-row filter may not leave previously selected track metadata in
# the accessible detail value. Require an independent native+MSAA host oracle.
AUDIT_UIA = (ROOT / "tests/assert_real_audit_uia.ps1").read_text(encoding="utf-8")
assert "PRODUCTION_AUDIT_HIDDEN_RESULT_MSAA_CLEAR_" in AUDIT_UIA
assert "GetNativeAccessibleValue($detailHwnd)" in AUDIT_UIA
assert "0x1032" in AUDIT_UIA
assert "$detailValue -ceq 'No result selected.'" in AUDIT_UIA

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

# Persisted column settings are untrusted. Parsing must consume complete
# numeric tokens and validate visibility + permutation before mutating UI.
assert "std::stoi(part, &consumed)" in UI
assert "consumed != part.size()" in UI
assert "std::stoul(mask_text, &consumed, 10)" in UI
assert "consumed == mask_text.size()" in UI
assert "parsed_mask >> column_count" in UI
assert "valid_order" in UI
assert "order = parsed_order" in UI

# Empty scope must invalidate old physical targets before returning, rather
# than leaving the previous selection available to the Properties command.
EMPTY_SCOPE = UI.split("if (state->source_items.get_count() == 0) {", 1)[1].split("\n        m_scan = state;", 1)[0]
assert EMPTY_SCOPE.index("m_rows.clear();") < EMPTY_SCOPE.index("refresh_list(false);")
assert EMPTY_SCOPE.index("refresh_list(false);") < EMPTY_SCOPE.index("return;")
assert "update_edit_enabled();" in UI.split("void refresh_list(bool preserve_selection)", 1)[1]
assert "if (m_scan) return;" in UI.split("void edit_selected_physical()", 1)[1]

# A nonempty replacement scan must also hide previous physical targets and
# accessible detail before the worker is started, not just disable Properties.
NONEMPTY_START = UI.split('void on_start_scan() {', 1)[1].split('state->logger.standard(', 1)[0]
assert NONEMPTY_START.index('m_rows.clear();', NONEMPTY_START.index('if (state->source_items.get_count() == 0)')) < NONEMPTY_START.index('m_scan = state;')
assert NONEMPTY_START.index('refresh_list(false);', NONEMPTY_START.index('if (state->source_items.get_count() == 0)')) < NONEMPTY_START.index('m_scan = state;')

# A failed library enumeration is a scope transition, not permission to keep
# the old physical target selected or enabled for Properties.
SCAN_ENTRY = UI.split("void on_start_scan() {", 1)[1].split("if (state->source_items.get_count() == 0)", 1)[0]
# The CI-only probe has an early return before the normal show_error path.
# Inspect the complete catch body up to the next scope branch.
ENUMERATION_FAILURE = SCAN_ENTRY.split("catch (const std::exception& error) {", 1)[1].split("\n        } else {", 1)[0]
assert ENUMERATION_FAILURE.index("m_rows.clear();") < ENUMERATION_FAILURE.index("refresh_list(false);")
assert ENUMERATION_FAILURE.index("refresh_list(false);") < ENUMERATION_FAILURE.index('show_error("The Media Library could not be enumerated."')
assert "EnableWindow(GetDlgItem(m_window, IDC_AUDIT_RESCAN), TRUE);" in ENUMERATION_FAILURE
assert "EnableWindow(GetDlgItem(m_window, IDC_AUDIT_CANCEL), FALSE);" in ENUMERATION_FAILURE
assert "update_edit_enabled();" in UI.split("void refresh_list(bool preserve_selection)", 1)[1]

# A scope change during an active scan invalidates physical targets before
# asynchronous cancellation can complete.
PENDING_SWITCH = UI.split("void request_scope(audit_scope scope,", 1)[1].split("void on_start_scan()", 1)[0].split("if (m_scan) {", 1)[1].split("return;", 1)[0]
assert PENDING_SWITCH.index("m_pending_scan = true;") < PENDING_SWITCH.index("m_rows.clear();")
assert PENDING_SWITCH.index("m_rows.clear();") < PENDING_SWITCH.index("refresh_list(false);")
assert PENDING_SWITCH.index("refresh_list(false);") < PENDING_SWITCH.index("cancel_scan();")
