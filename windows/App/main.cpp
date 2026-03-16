#ifdef _WIN32

#include "HotkeyManager.hpp"
#include "TrayIcon.hpp"
#include "OverlayWindow.hpp"

#include "../../src/utils/config_manager.hpp"
#include "../../src/daemon/mode_manager.hpp"
#include "../../src/daemon/recording_coordinator.hpp"
#include "../../src/daemon/window_tracker.hpp"
#include "../../src/utils/clipboard.hpp"

#include <windows.h>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <string>

using namespace sasayaku;

class SasayakuApp {
public:
    bool initialize(HINSTANCE hInstance) {
        hInstance_ = hInstance;

        // Create hidden message window
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = MessageWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"SasayakuMsg";
        RegisterClassExW(&wc);

        msgHwnd_ = CreateWindowExW(0, L"SasayakuMsg", L"", 0,
            0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, this);

        // Load config
        configManager_ = std::make_unique<ConfigManager>();
        if (!configManager_->load()) {
            std::cout << "Creating default configuration..." << std::endl;
            configManager_->initialize_defaults();
            auto& config = configManager_->get_mutable_config();
            config.whisper.model_path = configManager_->get_data_dir() + "\\models\\ggml-large-v3-turbo.bin";
            configManager_->save();
        }
        std::cout << "Configuration loaded from: " << configManager_->get_config_path() << std::endl;

        // Initialize mode manager
        modeManager_ = std::make_unique<ModeManager>();
        modeManager_->initialize(configManager_.get());

        // Initialize recording coordinator
        coordinator_ = std::make_unique<RecordingCoordinator>();
        if (!coordinator_->initialize(configManager_.get(), modeManager_.get())) {
            std::cerr << "Failed to initialize recording coordinator" << std::endl;
            // Continue anyway — user can fix settings
        } else {
            engineReady_ = true;
        }

        // Window tracker
        windowTracker_ = std::make_unique<WindowTracker>();
        windowTracker_->initialize();

        // Hotkey
        hotkey_.registerHotkey(msgHwnd_);
        hotkey_.onHotkeyPressed = [this]() { toggleRecording(); };

        // Overlay
        overlay_ = std::make_unique<OverlayWindow>();
        overlay_->create(hInstance);
        overlay_->onEscape = [this]() { cancelRecording(); };
        overlay_->onConfirm = [this]() { confirmAndCopy(); };

        // Tray icon
        HICON icon = LoadIcon(nullptr, IDI_APPLICATION);
        tray_.create(msgHwnd_, icon);

        std::cout << "Sasayaku started (Windows)" << std::endl;
        return true;
    }

    int run() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        tray_.destroy();
        hotkey_.unregisterHotkey(msgHwnd_);
        return (int)msg.wParam;
    }

private:
    HINSTANCE hInstance_ = nullptr;
    HWND msgHwnd_ = nullptr;
    bool engineReady_ = false;
    bool isRecording_ = false;

    std::unique_ptr<ConfigManager> configManager_;
    std::unique_ptr<ModeManager> modeManager_;
    std::unique_ptr<RecordingCoordinator> coordinator_;
    std::unique_ptr<WindowTracker> windowTracker_;
    std::unique_ptr<OverlayWindow> overlay_;
    HotkeyManager hotkey_;
    TrayIcon tray_;

    // Waveform data
    std::vector<float> waveformLevels_;
    static constexpr int MAX_WAVEFORM = 20;

    void toggleRecording() {
        if (!engineReady_) return;

        if (isRecording_) {
            stopRecording();
        } else if (overlay_->isVisible() && overlay_->getResultText().length() > 0) {
            confirmAndCopy();
        } else {
            startRecording();
        }
    }

    void startRecording() {
        if (!engineReady_ || isRecording_) return;

        // Auto-switch mode
        std::string activeApp = windowTracker_->get_active_app_id();
        if (!activeApp.empty()) {
            std::string modeForApp = modeManager_->get_mode_for_app(activeApp);
            if (modeForApp != modeManager_->get_current_mode()) {
                modeManager_->set_current_mode(modeForApp);
            }
        }

        if (coordinator_->start_recording()) {
            isRecording_ = true;
            waveformLevels_.clear();
            overlay_->setState(OverlayState::Recording);
            overlay_->setModeName(modeManager_->get_current_mode());
            overlay_->show();

            // Start waveform update timer
            SetTimer(msgHwnd_, 2, 50, nullptr);
        }
    }

    void stopRecording() {
        if (!isRecording_) return;

        isRecording_ = false;
        KillTimer(msgHwnd_, 2);
        overlay_->setState(OverlayState::Processing);

        coordinator_->stop_recording([this](const std::string& text, bool success) {
            // Post to main thread
            auto* result = new std::pair<std::string, bool>(text, success);
            PostMessage(msgHwnd_, WM_APP + 10, 0, reinterpret_cast<LPARAM>(result));
        });
    }

    void onTranscriptionComplete(const std::string& text, bool success) {
        if (success && !text.empty()) {
            Clipboard::set_text(text);
            overlay_->setResultText(text);
            overlay_->setState(OverlayState::Done);
            // Activate overlay so edit control gets focus
            SetForegroundWindow(overlay_->getHwnd());
        } else {
            overlay_->hide();
        }
    }

    void confirmAndCopy() {
        std::string text = overlay_->getResultText();
        if (!text.empty()) {
            Clipboard::set_text(text);
            std::cout << "Copied to clipboard (" << text.length() << " chars)" << std::endl;
        }
        overlay_->hide();
    }

    void cancelRecording() {
        if (isRecording_) {
            coordinator_->stop_recording([](const std::string&, bool) {});
            isRecording_ = false;
            KillTimer(msgHwnd_, 2);
        }
        overlay_->hide();
    }

    void updateWaveform() {
        if (!isRecording_) return;
        float level = coordinator_->get_audio_level();
        waveformLevels_.push_back(level);
        if ((int)waveformLevels_.size() > MAX_WAVEFORM) {
            waveformLevels_.erase(waveformLevels_.begin());
        }
        overlay_->setWaveformLevels(waveformLevels_);
    }

    void handleTrayCommand(int cmd) {
        switch (cmd) {
        case TrayIcon::CMD_TOGGLE:
            toggleRecording();
            break;
        case TrayIcon::CMD_SETTINGS:
            // TODO: Show settings dialog
            break;
        case TrayIcon::CMD_QUIT:
            PostQuitMessage(0);
            break;
        }
    }

    static LRESULT CALLBACK MessageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        SasayakuApp* self = nullptr;

        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            self = static_cast<SasayakuApp*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<SasayakuApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (self) {
            switch (msg) {
            case WM_HOTKEY:
                if (wParam == HotkeyManager::HOTKEY_ID) {
                    self->toggleRecording();
                }
                return 0;

            case TrayIcon::WM_TRAYICON:
                if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                    self->tray_.showContextMenu(hwnd, [self](int cmd) {
                        self->handleTrayCommand(cmd);
                    });
                }
                return 0;

            case WM_TIMER:
                if (wParam == 2) {
                    self->updateWaveform();
                }
                return 0;

            case WM_APP + 10: {
                // Transcription result from background thread
                auto* result = reinterpret_cast<std::pair<std::string, bool>*>(lParam);
                if (result) {
                    self->onTranscriptionComplete(result->first, result->second);
                    delete result;
                }
                return 0;
            }

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            }
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Enable console output for debugging
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    SasayakuApp app;
    if (!app.initialize(hInstance)) {
        MessageBoxW(nullptr, L"Failed to initialize Sasayaku", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    int result = app.run();

    CoUninitialize();
    return result;
}

#endif // _WIN32
