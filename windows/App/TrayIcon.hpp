#pragma once
#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>
#include <functional>

namespace sasayaku {

class TrayIcon {
public:
    bool create(HWND hwnd, HICON icon);
    void destroy();
    void setTooltip(const wchar_t* text);
    void showContextMenu(HWND hwnd, const std::function<void(int)>& onCommand);

    static constexpr UINT WM_TRAYICON = WM_APP + 1;

    // Menu item IDs
    static constexpr int CMD_TOGGLE = 1001;
    static constexpr int CMD_SETTINGS = 1002;
    static constexpr int CMD_QUIT = 1003;

private:
    NOTIFYICONDATAW nid_ = {};
    bool created_ = false;
};

} // namespace sasayaku

#endif
