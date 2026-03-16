#pragma once

#include <string>

namespace sasayaku {

class WindowTracker {
public:
    WindowTracker();
    ~WindowTracker();

    // Initialize window tracking
    bool initialize();

    // Get currently focused window's app ID
    std::string get_active_app_id();

private:
    // TODO: Implement using wlr-foreign-toplevel-management protocol
    // For now, we'll use a simple fallback
};

} // namespace sasayaku
