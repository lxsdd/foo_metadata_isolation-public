"""Static coverage and safety contract for Phase-8 hover tooltips."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src/foo_metadata_isolation"
TOOLTIPS_H = (SRC / "ui_tooltips.h").read_text(encoding="utf-8")
TOOLTIPS = (SRC / "ui_tooltips.cpp").read_text(encoding="utf-8")
PREFERENCES = (SRC / "preferences.cpp").read_text(encoding="utf-8")
PREFERENCES_H = (SRC / "preferences.h").read_text(encoding="utf-8")
SESSION = (SRC / "virtual_edit_session.cpp").read_text(encoding="utf-8")
CONTEXT = (SRC / "context_menu.cpp").read_text(encoding="utf-8")
DIAGNOSTICS = (SRC / "diagnostics.cpp").read_text(encoding="utf-8")
PROJECT = (SRC / "foo_metadata_isolation.vcxproj").read_text(encoding="utf-8")


def definitions(symbol: str) -> dict[str, str]:
    start = TOOLTIPS.index(f"constexpr dialog_tooltip_definition {symbol}[]")
    end = TOOLTIPS.index("};", start)
    block = TOOLTIPS[start:end]
    pairs = re.findall(r'\{(IDC_[A-Z0-9_]+|IDOK|IDCANCEL), L"([^"]+)"\}', block)
    result = dict(pairs)
    assert len(result) == len(pairs), f"duplicate tooltip control in {symbol}"
    return result


preferences = definitions("k_preferences_tooltips")
timing = definitions("k_timing_tooltips")
audit = definitions("k_physical_audit_tooltips")

assert set(preferences) == {
    "IDC_PROTECTION_STATUS", "IDC_PROTECTION_DETAIL", "IDC_VIRTUAL_EDITING_DETAIL",
    "IDC_COMMIT_DETAIL", "IDC_APPLY_SUPPORTED_CHANGES", "IDC_STRICT_ALL_OR_NOTHING",
    "IDC_LOGGING_STANDARD", "IDC_LOGGING_VERBOSE", "IDC_DISABLE_WRITE_GUARD",
    "IDC_DIAGNOSTICS_DETAIL",
}
assert set(timing) == {
    "IDC_TIMING_TRACK", "IDC_TIMING_DIRECTIVE", "IDC_TIMING_CURRENT",
    "IDC_TIMING_VALUE", "IDC_TIMING_REMOVE", "IDOK", "IDCANCEL",
}

assert set(audit) == {
    "IDC_AUDIT_STATUS", "IDC_AUDIT_PROGRESS", "IDC_AUDIT_FILTER", "IDC_AUDIT_RESULTS",
    "IDC_AUDIT_RESCAN", "IDC_AUDIT_CANCEL", "IDC_AUDIT_EDIT_PHYSICAL",
    "IDC_AUDIT_RESET_COLUMNS", "IDC_AUDIT_CLOSE",
}

for control, text in preferences.items() | timing.items() | audit.items():
    assert len(text.split()) >= 8, (control, text)
    lowered = text.casefold()
    for internal in (
        "shadow", "projection", "postimage", "transaction plan",
        "stale fingerprint", "committable", "known_skippable",
    ):
        assert internal not in lowered, (control, internal)

standard = preferences["IDC_LOGGING_STANDARD"].casefold()
assert "session result" in standard and "rejection reasons" in standard
assert "detailed pipeline diagnostics are omitted" in standard
verbose = preferences["IDC_LOGGING_VERBOSE"].casefold()
assert "pipeline" in verbose and "performance timings" in verbose
assert "credentials are not recorded" in verbose
partial = preferences["IDC_APPLY_SUPPORTED_CHANGES"].casefold()
assert "supported change" in partial and "unchanged" in partial
assert "unknown or blocking changes" in partial and "reject" in partial
strict = preferences["IDC_STRICT_ALL_OR_NOTHING"].casefold()
assert "complete operation" in strict and "without loss" in strict
assert "original file" in strict and "unchanged" in strict
guard = preferences["IDC_DISABLE_WRITE_GUARD"].casefold()
assert "turns off protection" in guard and "controlled diagnostics" in guard

for token in (
    "TOOLTIPS_CLASSW", "TTF_IDISHWND | TTF_SUBCLASS", "TTM_ADDTOOLW",
    "TTM_SETMAXTIPWIDTH", "TTM_SETDELAYTIME", "MulDiv(360",
):
    assert token in TOOLTIPS
assert "m_tooltips.initialize(m_hWnd, preferences_tooltip_definitions())" in PREFERENCES
assert "m_dark.AddCtrlAuto(m_tooltips.window())" in PREFERENCES
assert "m_tooltips.update_dpi(m_hWnd)" in PREFERENCES
assert "dialog_tooltips m_tooltips" in PREFERENCES_H
assert "state->tooltips.initialize(window, timing_tooltip_definitions())" in SESSION
assert "state->dark.AddCtrlAuto(state->tooltips.window())" in SESSION
assert "state->tooltips.update_dpi(window)" in SESSION
assert "physical_audit_tooltip_definitions()" in TOOLTIPS
AUDIT_UI = (SRC / "physical_metadata_audit_ui.cpp").read_text(encoding="utf-8")
assert "LVS_EX_INFOTIP" in AUDIT_UI
assert "m_tooltips.add_control(" in AUDIT_UI
assert "Click a header to sort." in AUDIT_UI
assert "Right-click a header to show or hide columns." in AUDIT_UI

assert "bool get_item_description" in CONTEXT
description_block = CONTEXT.split("bool get_item_description", 1)[1].split(
    "bool context_get_display", 1
)[0]
assert description_block.count("output =") == 9
for internal in ("deduplicated", "shadow diff", "allowlisted", "subsong 0"):
    assert internal not in description_block.casefold()
assert "TaskDialog owns these native buttons and exposes no per-button tooltip API" in SESSION
assert "TaskDialogIndirect" in SESSION and "popup_message_v3::query_t" in SESSION

for token in (
    "TTM_RELAYEVENT", "SendInput", "PostMessageW", "WM_MOUSELEAVE",
    "IsWindowVisible",
    "\\u00fc", "\\u0395", "\\u65e5", "\\U0001D11E",
    "/metadata-isolation-probe-tooltip-contract",
    "/metadata-isolation-probe-tooltip-contract-dark",
):
    assert token in TOOLTIPS + DIAGNOSTICS
assert 'ui_tooltips.cpp"' in PROJECT and 'ui_tooltips.h"' in PROJECT

print("Phase-8 tooltip coverage and UI safety contract passed.")
