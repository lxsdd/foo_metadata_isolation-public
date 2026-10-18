#pragma once

#include "guid.h"
#include "resource.h"
#include "ui_tooltips.h"

namespace metadata_isolation {

extern cfg_bool cfg_enabled;
extern cfg_bool cfg_apply_supported_changes;
extern cfg_bool cfg_verbose_logging;

#ifdef FB2K_ENABLE_TEST_PROBES
void force_preferences_dark_for_probe();
#endif

class preferences_page_instance_impl
    : public preferences_page_instance,
      public CDialogImpl<preferences_page_instance_impl> {
public:
    preferences_page_instance_impl(preferences_page_callback::ptr callback);

    enum { IDD = IDD_COMPONENT_PREFERENCES };

    t_uint32 get_state() override;
    void apply() override;
    void reset() override;
    HWND get_wnd() override { return m_hWnd; }

    BOOL OnInitDialog(CWindow, LPARAM);
    void OnChanged(UINT, int, CWindow);
    LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);

    BEGIN_MSG_MAP_EX(preferences_page_instance_impl)
        MSG_WM_INITDIALOG(OnInitDialog)
        COMMAND_HANDLER_EX(IDC_APPLY_SUPPORTED_CHANGES, BN_CLICKED, OnChanged)
        COMMAND_HANDLER_EX(IDC_STRICT_ALL_OR_NOTHING, BN_CLICKED, OnChanged)
        COMMAND_HANDLER_EX(IDC_LOGGING_STANDARD, BN_CLICKED, OnChanged)
        COMMAND_HANDLER_EX(IDC_LOGGING_VERBOSE, BN_CLICKED, OnChanged)
        COMMAND_HANDLER_EX(IDC_DISABLE_WRITE_GUARD, BN_CLICKED, OnChanged)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
    END_MSG_MAP()

private:
    bool has_changed() const;
    void refresh_protection_status();

    preferences_page_callback::ptr m_callback;
    fb2k::CCoreDarkModeHooks m_dark;
    dialog_tooltips m_tooltips;
};

class preferences_page_impl
    : public ::preferences_page_impl<preferences_page_instance_impl> {
public:
    const char* get_name() override { return "Metadata Isolation"; }
    GUID get_guid() override { return guids::preferences_page; }
    GUID get_parent_guid() override { return preferences_page::guid_tools; }
};

} // namespace metadata_isolation

