#include "window_tracker.hpp"
#include <cstdlib>
#include <array>
#include <memory>

namespace sasayaku {

WindowTracker::WindowTracker() {
}

WindowTracker::~WindowTracker() {
}

bool WindowTracker::initialize() {
    // TODO: Implement proper Wayland window tracking
    return true;
}

std::string WindowTracker::get_active_app_id() {
    // Simple fallback: try to use gdbus to get GNOME Shell's active window
    std::string result;
    std::array<char, 128> buffer;

    const char* cmd = "gdbus call --session --dest org.gnome.Shell "
                      "--object-path /org/gnome/Shell "
                      "--method org.gnome.Shell.Eval "
                      "\"global.display.focus_window.get_wm_class()\" 2>/dev/null";

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Parse the result to extract the app ID
    // Result format is typically: (true, '"AppName"')
    size_t start = result.find("\"");
    size_t end = result.rfind("\"");

    if (start != std::string::npos && end != std::string::npos && start < end) {
        return result.substr(start + 1, end - start - 1);
    }

    return "";
}

} // namespace sasayaku
