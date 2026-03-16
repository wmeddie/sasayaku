#ifdef _WIN32

#include "TrayIcon.hpp"

namespace sasayaku {

bool TrayIcon::create(HWND hwnd, HICON icon) {
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = icon;
    wcscpy_s(nid_.szTip, L"Sasayaku");
    created_ = Shell_NotifyIconW(NIM_ADD, &nid_);
    return created_;
}

void TrayIcon::destroy() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
}

void TrayIcon::setTooltip(const wchar_t* text) {
    if (!created_) return;
    wcscpy_s(nid_.szTip, text);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::showContextMenu(HWND hwnd, const std::function<void(int)>& onCommand) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, CMD_TOGGLE, L"Toggle Recording\tAlt+Space");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, CMD_SETTINGS, L"Settings...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, CMD_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd && onCommand) {
        onCommand(cmd);
    }
}

} // namespace sasayaku

#endif
