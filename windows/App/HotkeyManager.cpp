#ifdef _WIN32

#include "HotkeyManager.hpp"
#include <iostream>

namespace sasayaku {

bool HotkeyManager::registerHotkey(HWND hwnd) {
    // Alt+Space
    if (RegisterHotKey(hwnd, HOTKEY_ID, MOD_ALT, VK_SPACE)) {
        std::cout << "Global hotkey registered: Alt+Space" << std::endl;
        return true;
    }
    std::cerr << "Failed to register hotkey" << std::endl;
    return false;
}

void HotkeyManager::unregisterHotkey(HWND hwnd) {
    UnregisterHotKey(hwnd, HOTKEY_ID);
}

} // namespace sasayaku

#endif
