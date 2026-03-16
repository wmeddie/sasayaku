#ifdef __APPLE__

#include "../../src/utils/auto_paste.hpp"
#include "../../src/utils/clipboard.hpp"
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>
#include <unistd.h>

namespace sasayaku {

bool AutoPaste::is_available() {
    // Check if we have accessibility permission (needed for CGEvent)
    return AXIsProcessTrusted();
}

std::string AutoPaste::get_method() {
    return "cgevent";
}

bool AutoPaste::type_text(const std::string& text) {
    if (!is_available()) {
        return false;
    }

    // Use clipboard + paste approach (more reliable than per-character CGEvent)
    Clipboard::set_text(text);
    usleep(100000);  // 100ms delay
    return paste_from_clipboard();
}

bool AutoPaste::paste_from_clipboard() {
    if (!is_available()) {
        return false;
    }

    // Small delay to let window focus settle
    usleep(100000);  // 100ms

    // Simulate Cmd+V using CGEvent
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) {
        return false;
    }

    // Key code for 'V' is 9
    CGEventRef keyDown = CGEventCreateKeyboardEvent(source, (CGKeyCode)9, true);
    CGEventRef keyUp = CGEventCreateKeyboardEvent(source, (CGKeyCode)9, false);

    if (!keyDown || !keyUp) {
        if (keyDown) CFRelease(keyDown);
        if (keyUp) CFRelease(keyUp);
        CFRelease(source);
        return false;
    }

    // Set Command modifier
    CGEventSetFlags(keyDown, kCGEventFlagMaskCommand);
    CGEventSetFlags(keyUp, kCGEventFlagMaskCommand);

    // Post events
    CGEventPost(kCGHIDEventTap, keyDown);
    CGEventPost(kCGHIDEventTap, keyUp);

    CFRelease(keyDown);
    CFRelease(keyUp);
    CFRelease(source);

    return true;
}

} // namespace sasayaku

#endif // __APPLE__
