#ifdef _WIN32

#include "../../src/utils/clipboard.hpp"
#include "WinUtils.hpp"
#include <windows.h>

namespace sasayaku {

std::string Clipboard::get_text() {
    if (!OpenClipboard(nullptr)) return "";

    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pszText) {
            result = wide_to_utf8(pszText);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

bool Clipboard::set_text(const std::string& text) {
    if (!OpenClipboard(nullptr)) return false;

    EmptyClipboard();
    std::wstring wtext = utf8_to_wide(text);
    size_t size = (wtext.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
    memcpy(pMem, wtext.c_str(), size);
    GlobalUnlock(hMem);

    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
}

bool Clipboard::has_text() {
    return IsClipboardFormatAvailable(CF_UNICODETEXT);
}

void Clipboard::clear() {
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        CloseClipboard();
    }
}

} // namespace sasayaku

#endif // _WIN32
