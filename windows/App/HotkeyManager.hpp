#pragma once
#ifdef _WIN32

#include <windows.h>
#include <functional>

namespace sasayaku {

class HotkeyManager {
public:
    // Register Alt+Space as global hotkey on given window
    bool registerHotkey(HWND hwnd);
    void unregisterHotkey(HWND hwnd);

    // Call from WM_HOTKEY handler
    std::function<void()> onHotkeyPressed;

    static constexpr int HOTKEY_ID = 1;
};

} // namespace sasayaku

#endif
