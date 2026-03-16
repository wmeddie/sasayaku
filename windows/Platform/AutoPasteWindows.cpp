#ifdef _WIN32

#include "../../src/utils/auto_paste.hpp"
#include "../../src/utils/clipboard.hpp"
#include <windows.h>

namespace sasayaku {

bool AutoPaste::is_available() {
    return true;  // SendInput is always available on Windows
}

std::string AutoPaste::get_method() {
    return "sendinput";
}

bool AutoPaste::type_text(const std::string& text) {
    Clipboard::set_text(text);
    Sleep(100);
    return paste_from_clipboard();
}

bool AutoPaste::paste_from_clipboard() {
    Sleep(100);

    // Simulate Ctrl+V
    INPUT inputs[4] = {};

    // Ctrl down
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // V down
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    // V up
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Ctrl up
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    UINT sent = SendInput(4, inputs, sizeof(INPUT));
    return sent == 4;
}

} // namespace sasayaku

#endif // _WIN32
