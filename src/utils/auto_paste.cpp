#include "auto_paste.hpp"
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

namespace sasayaku {

static bool check_command_exists(const char* cmd) {
    std::string check = "which ";
    check += cmd;
    check += " > /dev/null 2>&1";
    return system(check.c_str()) == 0;
}

static std::string detected_method;

bool AutoPaste::is_available() {
    if (check_command_exists("ydotool")) {
        detected_method = "ydotool";
        return true;
    }

    if (check_command_exists("wtype")) {
        detected_method = "wtype";
        return true;
    }

    if (check_command_exists("xdotool")) {
        detected_method = "xdotool";
        return true;
    }

    return false;
}

std::string AutoPaste::get_method() {
    if (detected_method.empty()) {
        is_available();
    }
    return detected_method;
}

bool AutoPaste::type_text(const std::string& text) {
    if (!is_available()) {
        return false;
    }

    // Delay to let the user release the hotkey and window be ready
    usleep(300000);  // 300ms

    if (detected_method == "ydotool") {
        // ydotool type is preferred for Wayland
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            execlp("ydotool", "ydotool", "type", "--", text.c_str(), nullptr);
            // If execlp returns, it failed
            _exit(1);
        } else if (pid > 0) {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status) == 0;
            }
            return false;
        }
        return false;
    }

    if (detected_method == "wtype") {
        // wtype is another Wayland option
        pid_t pid = fork();
        if (pid == 0) {
            execlp("wtype", "wtype", text.c_str(), nullptr);
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        return false;
    }

    if (detected_method == "xdotool") {
        // xdotool for X11
        pid_t pid = fork();
        if (pid == 0) {
            execlp("xdotool", "xdotool", "type", "--clearmodifiers", text.c_str(), nullptr);
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        return false;
    }

    return false;
}

bool AutoPaste::paste_from_clipboard() {
    if (!is_available()) {
        return false;
    }

    // Small delay
    usleep(100000);  // 100ms

    if (detected_method == "ydotool") {
        pid_t pid = fork();
        if (pid == 0) {
            // Simulate Ctrl+V
            execlp("ydotool", "ydotool", "key", "29:1", "47:1", "47:0", "29:0", nullptr);
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        return false;
    }

    if (detected_method == "wtype") {
        pid_t pid = fork();
        if (pid == 0) {
            execlp("wtype", "wtype", "-M", "ctrl", "-P", "v", "-m", "ctrl", nullptr);
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        return false;
    }

    if (detected_method == "xdotool") {
        pid_t pid = fork();
        if (pid == 0) {
            execlp("xdotool", "xdotool", "key", "--clearmodifiers", "ctrl+v", nullptr);
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        return false;
    }

    return false;
}

} // namespace sasayaku
