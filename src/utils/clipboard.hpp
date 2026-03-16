#pragma once

#include <string>

namespace sasayaku {

class Clipboard {
public:
    // Get clipboard text content
    static std::string get_text();

    // Set clipboard text content
    static bool set_text(const std::string& text);

    // Check if clipboard has text
    static bool has_text();

    // Clear clipboard
    static void clear();
};

} // namespace sasayaku
