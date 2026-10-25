#include "stdafx.h"
#include "preferences.h"
#include "write_guard.h"

namespace metadata_isolation {

cfg_bool cfg_enabled(guids::cfg_enabled, true);
cfg_bool cfg_apply_supported_changes(guids::cfg_apply_supported_changes, true);
cfg_bool cfg_verbose_logging(guids::cfg_verbose_logging, false);

#ifdef FB2K_ENABLE_TEST_PROBES
namespace {
bool g_force_preferences_dark = false;
}

void force_preferences_dark_for_probe() {
    g_force_preferences_dark = true;
}
#endif

preferences_page_instance_impl::preferences_page_instance_impl(
    preferences_page_callback::ptr callback)
    : m_callback(std::move(callback))
#ifdef FB2K_ENABLE_TEST_PROBES
    , m_dark(g_force_preferences_dark)
#endif
{}

BOOL preferences_page_instance_impl::OnInitDialog(CWindow, LPARAM) {
    m_dark.AddDialogWithControls(m_hWnd);
    if (m_tooltips.initialize(m_hWnd, preferences_tooltip_definitions())) {
        m_dark.AddCtrlAuto(m_tooltips.window());
    }
    CheckDlgButton(IDC_DISABLE_WRITE_GUARD, cfg_enabled ? BST_UNCHECKED : BST_CHECKED);
    CheckRadioButton(
        IDC_APPLY_SUPPORTED_CHANGES,
        IDC_STRICT_ALL_OR_NOTHING,
        cfg_apply_supported_changes ? IDC_APPLY_SUPPORTED_CHANGES : IDC_STRICT_ALL_OR_NOTHING);
    CheckRadioButton(
        IDC_LOGGING_STANDARD,
        IDC_LOGGING_VERBOSE,
        cfg_verbose_logging ? IDC_LOGGING_VERBOSE : IDC_LOGGING_STANDARD);
    refresh_protection_status();
    return TRUE;
}

bool preferences_page_instance_impl::has_changed() const {
    return
        (IsDlgButtonChecked(IDC_DISABLE_WRITE_GUARD) != BST_CHECKED) !=
            static_cast<bool>(cfg_enabled) ||
        (IsDlgButtonChecked(IDC_APPLY_SUPPORTED_CHANGES) == BST_CHECKED) !=
            static_cast<bool>(cfg_apply_supported_changes) ||
        (IsDlgButtonChecked(IDC_LOGGING_VERBOSE) == BST_CHECKED) !=
            static_cast<bool>(cfg_verbose_logging);
}

t_uint32 preferences_page_instance_impl::get_state() {
    t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
    if (has_changed()) state |= preferences_state::changed;
    return state;
}

void preferences_page_instance_impl::apply() {
    cfg_enabled = IsDlgButtonChecked(IDC_DISABLE_WRITE_GUARD) != BST_CHECKED;
    cfg_apply_supported_changes =
        IsDlgButtonChecked(IDC_APPLY_SUPPORTED_CHANGES) == BST_CHECKED;
    cfg_verbose_logging = IsDlgButtonChecked(IDC_LOGGING_VERBOSE) == BST_CHECKED;
    refresh_protection_status();
    if (m_callback.is_valid()) m_callback->on_state_changed();
}

void preferences_page_instance_impl::reset() {
    CheckDlgButton(IDC_DISABLE_WRITE_GUARD, BST_UNCHECKED);
    CheckRadioButton(
        IDC_APPLY_SUPPORTED_CHANGES,
        IDC_STRICT_ALL_OR_NOTHING,
        IDC_APPLY_SUPPORTED_CHANGES);
    CheckRadioButton(IDC_LOGGING_STANDARD, IDC_LOGGING_VERBOSE, IDC_LOGGING_STANDARD);
    refresh_protection_status();
    if (m_callback.is_valid()) m_callback->on_state_changed();
}

void preferences_page_instance_impl::OnChanged(UINT, int, CWindow) {
    refresh_protection_status();
    if (m_callback.is_valid()) m_callback->on_state_changed();
}

void preferences_page_instance_impl::refresh_protection_status() {
    const bool disabled = IsDlgButtonChecked(IDC_DISABLE_WRITE_GUARD) == BST_CHECKED;
    const wchar_t* text = L"Protection setting: Active";
    const wchar_t* detail =
        L"Direct virtual writes are blocked to protect Cue and physical metadata.";
    if (disabled) {
        text = L"Protection setting: Disabled (unsafe)";
        detail = L"Direct-write protection is disabled by the diagnostics setting below.";
    } else {
        switch (get_protection_status()) {
        case protection_status::active:
            text = L"Protection evidence: unsafe virtual write blocked";
            detail = L"A direct unsafe virtual write was blocked in this session. Check Track Info Filters for current activation.";
            break;
        case protection_status::unknown:
            text = L"Protection enabled; no unsafe block observed";
            detail =
                L"No unsafe write has been blocked yet. This is not proof of a fault; check Preferences > Track Info Filters.";
            break;
        case protection_status::inactive:
            // The dialog's pending state is authoritative while the user edits it.
            text = L"Protection setting: Active (pending Apply)";
            detail = L"Apply this setting to enable direct-write protection.";
            break;
        }
    }
    ::SetDlgItemTextW(m_hWnd, IDC_PROTECTION_STATUS, text);
    ::SetDlgItemTextW(m_hWnd, IDC_PROTECTION_DETAIL, detail);
}

LRESULT preferences_page_instance_impl::OnDpiChanged(UINT, WPARAM, LPARAM l_param, BOOL&) {
    const auto* suggested = reinterpret_cast<const RECT*>(l_param);
    if (suggested != nullptr) {
        SetWindowPos(nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOACTIVATE | SWP_NOZORDER);
    }
    m_tooltips.update_dpi(m_hWnd);
    return 0;
}

static preferences_page_factory_t<preferences_page_impl> g_preferences_page_factory;

} // namespace metadata_isolation

