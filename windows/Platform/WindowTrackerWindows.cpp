#ifdef _WIN32

#include "../../src/daemon/window_tracker.hpp"
#include "WinUtils.hpp"
#include <windows.h>
#include <psapi.h>
#include <algorithm>

namespace sasayaku {

WindowTracker::WindowTracker() {}
WindowTracker::~WindowTracker() {}

bool WindowTracker::initialize() {
    return true;
}

std::string WindowTracker::get_active_app_id() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "";

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return "";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return "";

    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    QueryFullProcessImageNameW(hProcess, 0, path, &size);
    CloseHandle(hProcess);

    // Extract just the filename
    std::wstring wpath(path);
    size_t pos = wpath.find_last_of(L"\\/");
    std::wstring filename = (pos != std::wstring::npos) ? wpath.substr(pos + 1) : wpath;

    std::string result = wide_to_utf8(filename);
    // Lowercase for matching
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

} // namespace sasayaku

#endif // _WIN32
