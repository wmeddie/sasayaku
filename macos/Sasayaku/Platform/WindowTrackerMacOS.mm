#ifdef __APPLE__

#include "../../src/daemon/window_tracker.hpp"
#import <AppKit/AppKit.h>

namespace sasayaku {

WindowTracker::WindowTracker() {
}

WindowTracker::~WindowTracker() {
}

bool WindowTracker::initialize() {
    return true;
}

std::string WindowTracker::get_active_app_id() {
    @autoreleasepool {
        NSRunningApplication* frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (frontApp && frontApp.bundleIdentifier) {
            return std::string([frontApp.bundleIdentifier UTF8String]);
        }
    }
    return "";
}

} // namespace sasayaku

#endif // __APPLE__
