#ifdef __APPLE__

#include "../../src/utils/clipboard.hpp"
#import <AppKit/AppKit.h>

namespace sasayaku {

std::string Clipboard::get_text() {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
        if (text) {
            return std::string([text UTF8String]);
        }
    }
    return "";
}

bool Clipboard::set_text(const std::string& text) {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        NSString* nsText = [NSString stringWithUTF8String:text.c_str()];
        return [pasteboard setString:nsText forType:NSPasteboardTypeString];
    }
}

bool Clipboard::has_text() {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        NSArray* types = [pasteboard types];
        return [types containsObject:NSPasteboardTypeString];
    }
}

void Clipboard::clear() {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
    }
}

} // namespace sasayaku

#endif // __APPLE__
