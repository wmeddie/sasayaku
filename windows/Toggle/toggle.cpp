#ifdef _WIN32

#include <windows.h>
#include <iostream>

// Simple CLI tool that sends a toggle command via named pipe
int main() {
    HANDLE hPipe = CreateFileW(
        L"\\\\.\\pipe\\sasayaku",
        GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Sasayaku is not running" << std::endl;
        return 1;
    }

    const char* cmd = "toggle";
    DWORD written = 0;
    WriteFile(hPipe, cmd, (DWORD)strlen(cmd), &written, nullptr);
    CloseHandle(hPipe);

    std::cout << "Toggle command sent" << std::endl;
    return 0;
}

#endif
