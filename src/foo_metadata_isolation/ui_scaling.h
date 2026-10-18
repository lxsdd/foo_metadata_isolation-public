#pragma once

#include <Windows.h>

namespace metadata_isolation::ui {

inline UINT detect_window_dpi(HWND window) {
    if (window != nullptr) {
        const HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32 != nullptr) {
            using get_dpi_for_window_t = UINT(WINAPI*)(HWND);
            const auto get_dpi_for_window = reinterpret_cast<get_dpi_for_window_t>(
                GetProcAddress(user32, "GetDpiForWindow"));
            if (get_dpi_for_window != nullptr) {
                const UINT dpi = get_dpi_for_window(window);
                if (dpi != 0) return dpi;
            }
        }
    }

    HWND dc_window = window;
    HDC dc = GetDC(dc_window);
    if (dc == nullptr) {
        dc_window = nullptr;
        dc = GetDC(nullptr);
    }
    if (dc == nullptr) return USER_DEFAULT_SCREEN_DPI;

    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(dc_window, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : USER_DEFAULT_SCREEN_DPI;
}

inline int scale_for_window_dpi(HWND window, int value) {
    return MulDiv(value, static_cast<int>(detect_window_dpi(window)), USER_DEFAULT_SCREEN_DPI);
}

inline int scale_between_dpi(int value, UINT old_dpi, UINT new_dpi) {
    if (old_dpi == 0) old_dpi = USER_DEFAULT_SCREEN_DPI;
    if (new_dpi == 0) new_dpi = USER_DEFAULT_SCREEN_DPI;
    return MulDiv(value, static_cast<int>(new_dpi), static_cast<int>(old_dpi));
}

} // namespace metadata_isolation::ui

