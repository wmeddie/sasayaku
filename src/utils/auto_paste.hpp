#pragma once

#include <string>

namespace sasayaku {

class AutoPaste {
public:
    // Type text into the focused window
    // Uses ydotool or similar tool to inject keystrokes
    static bool type_text(const std::string& text);

    // Simulate Ctrl+V paste
    static bool paste_from_clipboard();

    // Check if auto-paste is available
    static bool is_available();

    // Get method being used ("ydotool", "xdotool", etc.)
    static std::string get_method();
};

} // namespace sasayaku
