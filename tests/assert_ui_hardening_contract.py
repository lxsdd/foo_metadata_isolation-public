"""Static contract for the Phase-6 Preferences and dialog hardening."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src/foo_metadata_isolation"
PREFERENCES = (SRC / "preferences.cpp").read_text(encoding="utf-8")
PREFERENCES_H = (SRC / "preferences.h").read_text(encoding="utf-8")
RESOURCE = (SRC / "component.rc").read_text(encoding="utf-8")
RESOURCE_H = (SRC / "resource.h").read_text(encoding="utf-8")
GUIDS = (SRC / "guid.h").read_text(encoding="utf-8")
SESSION = (SRC / "virtual_edit_session.cpp").read_text(encoding="utf-8")
LOGGER = (SRC / "operation_log.cpp").read_text(encoding="utf-8")
DIAGNOSTICS = (SRC / "diagnostics.cpp").read_text(encoding="utf-8")
CONTEXT = (SRC / "context_menu.cpp").read_text(encoding="utf-8")


# The SDK requires both the hook and the advertised capability flag.
assert "m_dark.AddDialogWithControls(m_hWnd)" in PREFERENCES
assert "preferences_state::dark_mode_supported" in PREFERENCES
assert "state->dark.AddDialogWithControls(window)" in SESSION
assert SESSION.count("message == WM_DPICHANGED") >= 1
assert "MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)" in PREFERENCES_H
assert "/metadata-isolation-probe-open-preferences-dark" in DIAGNOSTICS
assert "force_preferences_dark_for_probe" in DIAGNOSTICS

# Protection is a status/invariant in the normal UI. Disabling it is an
# explicitly unsafe diagnostic action and preserves the existing cfg GUID.
assert 'GROUPBOX        "Protection"' in RESOURCE
assert '"Protection status: Active"' in RESOURCE
assert '"Disable direct-write protection (advanced, unsafe)"' in RESOURCE
assert "IDC_DISABLE_WRITE_GUARD" in PREFERENCES
assert "cfg_enabled(guids::cfg_enabled, true)" in PREFERENCES
assert "Block unsafe virtual MP3/FLAC writes" not in RESOURCE

# The Phase-5 policy is represented directly, with default safe partial commit
# and strict rejection as the alternative.
assert 'GROUPBOX        "Commit Behavior"' in RESOURCE
assert '"Apply supported changes; report skipped changes"' in RESOURCE
assert '"Reject the entire operation"' in RESOURCE
assert "cfg_apply_supported_changes(guids::cfg_apply_supported_changes, true)" in PREFERENCES

# Logging controls have real persisted behavior and never print metadata values
# or credentials: only fixed lifecycle text and numeric result counts.
assert 'GROUPBOX        "Logging"' in RESOURCE
assert '"Standard"' in RESOURCE and '"Verbose"' in RESOURCE
assert "cfg_verbose_logging(guids::cfg_verbose_logging, false)" in PREFERENCES
assert "cfg_verbose_logging" in LOGGER
assert "cfg_verbose_logging =" in PREFERENCES
assert "cfg_verbose_logging =" not in SESSION
guid_initializers = re.findall(r"inline constexpr GUID\s+\w+\s*=\s*(\{[^;]+\});", GUIDS)
assert len(guid_initializers) == len(set(guid_initializers))

# Every actionable preference has a stable resource ID and native keyboard
# navigation. Radio groups expose exactly one WS_GROUP starting point each.
for control in (
    "IDC_APPLY_SUPPORTED_CHANGES",
    "IDC_STRICT_ALL_OR_NOTHING",
    "IDC_LOGGING_STANDARD",
    "IDC_LOGGING_VERBOSE",
    "IDC_DISABLE_WRITE_GUARD",
):
    assert control in RESOURCE_H
    line = next(line for line in RESOURCE.splitlines() if control in line)
    assert "WS_TABSTOP" in line
assert RESOURCE.count("BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP") == 2

ids = re.findall(r"^#define\s+(IDC_[A-Z0-9_]+)\s+(\d+)$", RESOURCE_H, re.MULTILINE)
numbers = [number for _, number in ids]
assert len(numbers) == len(set(numbers))

# A live Win32/x64 TaskDialog exposed IDOK/Commit as its native default
# even when the SDK nDefaultButton was set to IDCANCEL. Always put the
# non-writing Discard button first as a safe fallback and keep the explicit
# default IDCANCEL. Never allow Commit to be the first custom action.
review = SESSION[SESSION.index("bool show_commit_dialog("):]
review = review[:review.index("struct timing_dialog_state")]
assert review.index('{ button_discard, L"Discard" }') < review.index('{ button_commit, L"Commit" }')
assert "constexpr int button_commit = 0x2001;" in review
assert "constexpr int button_discard = 0x2002;" in review
assert "config.nDefaultButton = button_discard;" in review
assert "int selected = button_discard;" in review
assert "TDF_ALLOW_DIALOG_CANCELLATION" in review
assert "return selected == button_commit;" in review
assert "return selected == IDOK;" not in review

# Do not advertise the not-yet-universal automatic-preview/completion model.
assert "Always show change preview" not in RESOURCE
assert "Apply safe changes automatically" not in RESOURCE

# Nine stable context actions remain registered; the ninth is the read-only Physical Metadata Audit. Recovery actions stay hidden
# from the normal menu until the generic completion boundary changes.
assert "unsigned get_num_items() override { return 9; }" in CONTEXT
assert '"Audit physical metadata for selection..."' in CONTEXT
assert "index == command_virtual_review || index == command_virtual_discard" in CONTEXT

print("Phase-6 UI hardening static contract assertions passed.")
