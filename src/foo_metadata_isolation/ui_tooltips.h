#pragma once

#include <span>

namespace metadata_isolation {

struct dialog_tooltip_definition {
    int control_id;
    const wchar_t* text;
};

std::span<const dialog_tooltip_definition> preferences_tooltip_definitions() noexcept;
std::span<const dialog_tooltip_definition> timing_tooltip_definitions() noexcept;
std::span<const dialog_tooltip_definition> physical_audit_tooltip_definitions() noexcept;

class dialog_tooltips {
public:
    dialog_tooltips() = default;
    ~dialog_tooltips();

    dialog_tooltips(const dialog_tooltips&) = delete;
    dialog_tooltips& operator=(const dialog_tooltips&) = delete;

    bool initialize(
        HWND dialog,
        std::span<const dialog_tooltip_definition> definitions) noexcept;
    bool add_control(HWND dialog, HWND control, const wchar_t* text) noexcept;
    void update_dpi(HWND dialog) noexcept;

    HWND window() const noexcept { return m_window; }
    size_t registered_count() const noexcept { return m_registered_count; }

private:
    void reset() noexcept;

    HWND m_window = nullptr;
    size_t m_registered_count = 0;
};

#if defined(FB2K_ENABLE_TEST_PROBES)
struct tooltip_runtime_probe_result {
    size_t requested_count = 0;
    size_t registered_count = 0;
    bool texts_match = false;
    bool distinct_mapping = false;
    bool parent_visible = false;
    bool parent_foreground = false;
    bool cursor_inside = false;
    bool mouse_event_sent = false;
    bool current_tool_present = false;
    bool forced_popup_visible = false;
    bool hover_appeared = false;
    bool hover_dismissed = false;
    bool unicode_roundtrip = false;
    bool dark_mode_requested = false;
    unsigned dpi = 0;
};

tooltip_runtime_probe_result run_tooltip_runtime_probe(bool dark_mode) noexcept;
#endif

} // namespace metadata_isolation
