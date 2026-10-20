#include "stdafx.h"
#include "resource.h"
#include "ui_scaling.h"
#include "ui_tooltips.h"

#include <commctrl.h>
#include <cwchar>

namespace metadata_isolation {
namespace {

constexpr dialog_tooltip_definition k_preferences_tooltips[] = {
    {IDC_PROTECTION_STATUS, L"Shows whether protection against unsafe direct edits is active. Keep this protection enabled when working with virtual Cue tracks."},
    {IDC_PROTECTION_DETAIL, L"Explains the current protection state. Direct edits to virtual Cue tracks can otherwise overwrite the embedded Cuesheet or physical file tags."},
    {IDC_VIRTUAL_EDITING_DETAIL, L"Use safe Virtual commands to change only selected Cue tracks. Metadata Isolation validates the result before replacing an original file."},
    {IDC_COMMIT_DETAIL, L"Choose how to handle a known change that the embedded Cuesheet cannot store without loss. Unknown or unsafe changes always reject the whole operation."},
    {IDC_APPLY_SUPPORTED_CHANGES, L"Applies every supported change and leaves only specifically qualified non-representable changes unchanged. Unknown or blocking changes still reject the whole operation."},
    {IDC_STRICT_ALL_OR_NOTHING, L"Applies changes only when the complete operation can be stored without loss. Otherwise every original file remains unchanged."},
    {IDC_LOGGING_STANDARD, L"Records the main user and support events, including the session result, rejection reasons and useful summaries. Detailed pipeline diagnostics are omitted."},
    {IDC_LOGGING_VERBOSE, L"Adds technical pipeline and performance timings for troubleshooting and support. File paths, metadata values and credentials are not recorded."},
    {IDC_DISABLE_WRITE_GUARD, L"Turns off protection against unsafe direct writes to virtual Cue tracks. Use only for controlled diagnostics because Cue and physical metadata may be rewritten."},
    {IDC_DIAGNOSTICS_DETAIL, L"Disabling protection affects write safety, not just the display. Leave it enabled for normal use."},
};


constexpr dialog_tooltip_definition k_physical_audit_tooltips[] = {
    {IDC_AUDIT_STATUS, L"Shows scan progress and the persistent result summary. The audit itself never writes media files."},
    {IDC_AUDIT_PROGRESS, L"Shows that the read-only Library or selection scan is active. The audit never changes tags while this indicator is running."},
    {IDC_AUDIT_FILTER, L"Filters the result grid. Problems includes EMPTY, INCOMPLETE, MISMATCH and REVIEW; All also shows files whose audited physical core tags are OK."},
    {IDC_AUDIT_RESULTS, L"One row per physical source with an embedded textual CUESHEET. Status is OK, EMPTY, INCOMPLETE, MISMATCH or REVIEW. Physical columns are subsong-0 tags; Cue columns show only unambiguous global Cue values. Missing core fields and Cue differences identify what needs attention."},
    {IDC_AUDIT_RESCAN, L"Scans the same Library or selection again. The scan is read-only and refreshes the result grid."},
    {IDC_AUDIT_CANCEL, L"Requests cancellation of the current read-only scan. No metadata is written."},
    {IDC_AUDIT_EDIT_PHYSICAL, L"Opens foobar2000 Properties for the selected physical source files. Changes made there affect physical metadata, not the audit result itself."},
    {IDC_AUDIT_RESET_COLUMNS, L"Restores default column order, widths and visibility for this audit view."},
    {IDC_AUDIT_CLOSE, L"Closes the audit window. Window geometry, filter and column layout are retained."},
};

constexpr dialog_tooltip_definition k_timing_tooltips[] = {
    {IDC_TIMING_TRACK, L"Identifies the existing Cue track being edited. Changing timing does not create, remove or reorder tracks."},
    {IDC_TIMING_DIRECTIVE, L"Chooses the Cue timing entry to edit. INDEX 01 is required; INDEX 00, INDEX 02 through 99 and PREGAP are optional when valid."},
    {IDC_TIMING_CURRENT, L"Shows the exact value currently stored in the Cuesheet, or <missing> when an optional timing entry does not exist."},
    {IDC_TIMING_VALUE, L"Enter an exact MM:SS:FF time at 75 frames per second. All affected track boundaries are checked before writing."},
    {IDC_TIMING_REMOVE, L"Removes the selected optional INDEX or PREGAP entry. The required INDEX 01 start position cannot be removed."},
    {IDOK, L"Validates the requested timing change and opens a review of its effects before anything is written."},
    {IDCANCEL, L"Closes the timing editor without changing the original file."},
};

unsigned window_dpi(HWND window) noexcept {
    try {
        const unsigned dpi = static_cast<unsigned>(ui::detect_window_dpi(window));
        return dpi == 0 ? 96 : dpi;
    } catch (...) {
        return 96;
    }
}

#if defined(FB2K_ENABLE_TEST_PROBES)
bool tooltip_text(
    HWND tooltip,
    HWND parent,
    HWND control,
    wchar_t* output,
    size_t output_count) noexcept {
    if (tooltip == nullptr || parent == nullptr || control == nullptr ||
        output == nullptr || output_count == 0) return false;
    output[0] = 0;
    TOOLINFOW info = {};
    info.cbSize = sizeof(info);
    info.hwnd = parent;
    info.uId = reinterpret_cast<UINT_PTR>(control);
    info.lpszText = output;
    SendMessageW(tooltip, TTM_GETTEXTW, output_count, reinterpret_cast<LPARAM>(&info));
    return output[0] != 0;
}
#endif

} // namespace

std::span<const dialog_tooltip_definition> preferences_tooltip_definitions() noexcept {
    return k_preferences_tooltips;
}

std::span<const dialog_tooltip_definition> timing_tooltip_definitions() noexcept {
    return k_timing_tooltips;
}

std::span<const dialog_tooltip_definition> physical_audit_tooltip_definitions() noexcept {
    return k_physical_audit_tooltips;
}

dialog_tooltips::~dialog_tooltips() {
    reset();
}

void dialog_tooltips::reset() noexcept {
    if (m_window != nullptr && IsWindow(m_window)) DestroyWindow(m_window);
    m_window = nullptr;
    m_registered_count = 0;
}

bool dialog_tooltips::initialize(
    HWND dialog,
    std::span<const dialog_tooltip_definition> definitions) noexcept {
    reset();
    try {
        if (dialog == nullptr || definitions.empty()) return false;
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_WIN95_CLASSES};
        if (!InitCommonControlsEx(&controls)) return false;
        m_window = CreateWindowExW(
            WS_EX_TOPMOST,
            TOOLTIPS_CLASSW,
            nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            dialog,
            nullptr,
            core_api::get_my_instance(),
            nullptr);
        if (m_window == nullptr) return false;
        SetWindowPos(
            m_window,
            HWND_TOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SendMessageW(m_window, TTM_SETDELAYTIME, TTDT_INITIAL, 450);
        update_dpi(dialog);

        for (const auto& definition : definitions) {
            const HWND control = GetDlgItem(dialog, definition.control_id);
            if (!add_control(dialog, control, definition.text)) {
                reset();
                return false;
            }
        }
        return true;
    } catch (...) {
        reset();
        return false;
    }
}

bool dialog_tooltips::add_control(
    HWND dialog,
    HWND control,
    const wchar_t* text) noexcept {
    try {
        if (m_window == nullptr || dialog == nullptr || control == nullptr ||
            text == nullptr || text[0] == 0) return false;
        TOOLINFOW info = {};
        info.cbSize = sizeof(info);
        info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = dialog;
        info.uId = reinterpret_cast<UINT_PTR>(control);
        info.lpszText = const_cast<wchar_t*>(text);
        if (!SendMessageW(m_window, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info))) {
            return false;
        }
        ++m_registered_count;
        return true;
    } catch (...) {
        return false;
    }
}

void dialog_tooltips::update_dpi(HWND dialog) noexcept {
    if (m_window == nullptr || dialog == nullptr) return;
    const int maximum_width = MulDiv(360, static_cast<int>(window_dpi(dialog)), 96);
    SendMessageW(m_window, TTM_SETMAXTIPWIDTH, 0, maximum_width);
}

#if defined(FB2K_ENABLE_TEST_PROBES)
tooltip_runtime_probe_result run_tooltip_runtime_probe(bool dark_mode) noexcept {
    tooltip_runtime_probe_result result;
    result.dark_mode_requested = dark_mode;
    POINT original_cursor = {};
    const bool restore_cursor = GetCursorPos(&original_cursor) != FALSE;
    constexpr wchar_t first_text[] =
        L"Gr\u00fc\u00dfe - \u0395\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac - "
        L"\u65e5\u672c\u8a9e - \U0001D11E";
    constexpr wchar_t second_text[] = L"A distinct second tooltip mapping.";
    const dialog_tooltip_definition definitions[] = {
        {1, first_text},
        {2, second_text},
    };
    result.requested_count = _countof(definitions);

    HWND parent = nullptr;
    try {
        parent = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            L"STATIC",
            L"Metadata Isolation tooltip probe",
            WS_POPUP,
            40,
            40,
            320,
            100,
            nullptr,
            nullptr,
            core_api::get_my_instance(),
            nullptr);
        if (parent == nullptr) return result;
        const HWND first = CreateWindowExW(
            0, L"BUTTON", L"First", WS_CHILD | WS_VISIBLE,
            10, 10, 120, 24, parent, reinterpret_cast<HMENU>(1),
            core_api::get_my_instance(), nullptr);
        const HWND second = CreateWindowExW(
            0, L"BUTTON", L"Second", WS_CHILD | WS_VISIBLE,
            10, 44, 120, 24, parent, reinterpret_cast<HMENU>(2),
            core_api::get_my_instance(), nullptr);
        if (first == nullptr || second == nullptr) {
            DestroyWindow(parent);
            return result;
        }

        dialog_tooltips tooltips;
        if (!tooltips.initialize(parent, definitions)) {
            DestroyWindow(parent);
            return result;
        }
        fb2k::CCoreDarkModeHooks dark(dark_mode);
        dark.AddDialogWithControls(parent);
        dark.AddCtrlAuto(tooltips.window());
        result.dpi = window_dpi(parent);
        result.registered_count = tooltips.registered_count();

        wchar_t first_buffer[256] = {};
        wchar_t second_buffer[256] = {};
        const bool first_read = tooltip_text(
            tooltips.window(), parent, first, first_buffer, _countof(first_buffer));
        const bool second_read = tooltip_text(
            tooltips.window(), parent, second, second_buffer, _countof(second_buffer));
        result.texts_match = first_read && second_read &&
            std::wcscmp(first_buffer, first_text) == 0 &&
            std::wcscmp(second_buffer, second_text) == 0;
        result.distinct_mapping = result.texts_match &&
            std::wcscmp(first_buffer, second_buffer) != 0;
        result.unicode_roundtrip = first_read && std::wcscmp(first_buffer, first_text) == 0;

        SendMessageW(tooltips.window(), TTM_SETDELAYTIME, TTDT_INITIAL, 10);
        SendMessageW(tooltips.window(), TTM_SETDELAYTIME, TTDT_AUTOPOP, 100);
        ShowWindow(parent, SW_SHOW);
        UpdateWindow(parent);
        SetForegroundWindow(parent);
        SetActiveWindow(parent);
        SetFocus(first);
        result.parent_visible = IsWindowVisible(parent) != FALSE;
        result.parent_foreground = GetForegroundWindow() == parent;
        RECT first_bounds = {};
        GetWindowRect(first, &first_bounds);
        SetCursorPos(
            first_bounds.left + (first_bounds.right - first_bounds.left) / 2,
            first_bounds.top + (first_bounds.bottom - first_bounds.top) / 2);
        POINT cursor = {};
        result.cursor_inside = GetCursorPos(&cursor) != FALSE &&
            PtInRect(&first_bounds, cursor) != FALSE;
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dx = 1;
        input.mi.dy = 0;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        result.mouse_event_sent = SendInput(1, &input, sizeof(input)) == 1;
        MSG relay = {};
        relay.hwnd = first;
        relay.message = WM_MOUSEMOVE;
        relay.lParam = MAKELPARAM(5, 5);
        relay.time = GetMessageTime();
        relay.pt = {5, 5};
        ClientToScreen(first, &relay.pt);
        PostMessageW(first, WM_MOUSEMOVE, 0, MAKELPARAM(5, 5));
        SendMessageW(tooltips.window(), TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(&relay));
        const ULONGLONG appear_deadline = GetTickCount64() + 1000;
        while (IsWindowVisible(tooltips.window()) == FALSE &&
            GetTickCount64() < appear_deadline) {
            MSG message = {};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(10);
        }
        TOOLINFOW active = {};
        active.cbSize = sizeof(active);
        result.current_tool_present = SendMessageW(
            tooltips.window(), TTM_GETCURRENTTOOLW, 0,
            reinterpret_cast<LPARAM>(&active)) != FALSE;
        result.hover_appeared = IsWindowVisible(tooltips.window()) != FALSE;
        if (!result.hover_appeared) {
            // Diagnostic only: an explicitly requested popup is NOT evidence
            // that a user mouse-over tooltip appeared. Keep hover=false.
            SendMessageW(tooltips.window(), TTM_POPUP, 0, 0);
            result.forced_popup_visible = IsWindowVisible(tooltips.window()) != FALSE;
            SendMessageW(tooltips.window(), TTM_POP, 0, 0);
        }
        SetCursorPos(first_bounds.right + 40, first_bounds.bottom + 40);
        SendMessageW(first, WM_MOUSELEAVE, 0, 0);
        const ULONGLONG dismiss_deadline = GetTickCount64() + 1000;
        while (IsWindowVisible(tooltips.window()) != FALSE &&
            GetTickCount64() < dismiss_deadline) {
            MSG message = {};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(10);
        }
        result.hover_dismissed = IsWindowVisible(tooltips.window()) == FALSE;
        ShowWindow(parent, SW_HIDE);
        DestroyWindow(parent);
        if (restore_cursor) SetCursorPos(original_cursor.x, original_cursor.y);
        return result;
    } catch (...) {
        if (restore_cursor) SetCursorPos(original_cursor.x, original_cursor.y);
        if (parent != nullptr) DestroyWindow(parent);
        return result;
    }
}
#endif

} // namespace metadata_isolation
