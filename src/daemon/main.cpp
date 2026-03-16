#include "dbus_service.hpp"
#include "mode_manager.hpp"
#include "window_tracker.hpp"
#include "recording_coordinator.hpp"
#include "../utils/config_manager.hpp"
#include "../ui/recording_window.hpp"
#include "../ui/settings_window.hpp"
#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>

using namespace sasayaku;

class SasayakuDaemon {
public:
    SasayakuDaemon() = default;

    bool initialize(int argc, char** argv) {
        // Initialize GTK for the UI
        gtk_init();

        // Load configuration
        config_manager_ = std::make_unique<ConfigManager>();
        if (!config_manager_->load()) {
            std::cout << "Creating default configuration..." << std::endl;
            config_manager_->initialize_defaults();

            // Set a default model path (user will need to adjust this)
            auto& config = config_manager_->get_mutable_config();
            config.whisper.model_path = config_manager_->get_data_dir() + "/models/ggml-large-v3-turbo.bin";

            config_manager_->save();
        }

        std::cout << "Configuration loaded from: " << config_manager_->get_config_path() << std::endl;

        // Check if whisper model exists
        const auto& whisper_config = config_manager_->get_config().whisper;
        if (whisper_config.model_path.empty()) {
            std::cerr << "Error: Whisper model path not configured!" << std::endl;
            std::cerr << "Please download a whisper model and set the path in:" << std::endl;
            std::cerr << config_manager_->get_config_path() << std::endl;
            return false;
        }

        // Initialize mode manager
        mode_manager_ = std::make_unique<ModeManager>();
        mode_manager_->initialize(config_manager_.get());

        // Initialize recording coordinator
        recording_coordinator_ = std::make_unique<RecordingCoordinator>();
        if (!recording_coordinator_->initialize(config_manager_.get(), mode_manager_.get())) {
            std::cerr << "Failed to initialize recording coordinator" << std::endl;
            return false;
        }

        // Initialize window tracker
        window_tracker_ = std::make_unique<WindowTracker>();
        window_tracker_->initialize();

        // Initialize D-Bus service
        dbus_service_ = std::make_unique<DBusService>();

        dbus_service_->set_start_recording_callback([this]() {
            this->start_recording();
        });

        dbus_service_->set_stop_recording_callback([this]() {
            this->stop_recording();
        });

        dbus_service_->set_toggle_recording_callback([this]() {
            this->toggle_recording();
        });

        dbus_service_->set_status_callback([this]() {
            return this->get_status();
        });

        if (!dbus_service_->initialize()) {
            std::cerr << "Failed to initialize D-Bus service" << std::endl;
            return false;
        }

        // Initialize recording window
        recording_window_ = std::make_unique<RecordingWindow>();

        // Collect modes for UI
        std::vector<ModeInfo> mode_infos;
        const auto& modes = config_manager_->get_config().modes;
        for (const auto& [id, mode] : modes) {
            ModeInfo info;
            info.id = id;
            info.name = mode.name;
            info.description = mode.description;
            mode_infos.push_back(info);
        }

        if (!recording_window_->initialize(mode_infos)) {
            std::cerr << "Failed to initialize recording window" << std::endl;
            return false;
        }

        // Set up window callbacks
        recording_window_->set_record_callback([this](bool is_recording) {
            if (is_recording) {
                this->start_recording();
            } else {
                this->stop_recording();
            }
        });

        recording_window_->set_mode_changed_callback([this](const std::string& mode_id) {
            this->mode_manager_->set_current_mode(mode_id);
            std::cout << "Mode changed to: " << mode_id << std::endl;
        });

        recording_window_->set_settings_callback([this]() {
            this->show_settings();
        });

        // Initialize settings window
        settings_window_ = std::make_unique<SettingsWindow>();
        if (!settings_window_->initialize()) {
            std::cerr << "Failed to initialize settings window" << std::endl;
            return false;
        }

        // Load current settings into window
        SettingsData current_settings;
        current_settings.api_base_url = config_manager_->get_config().api.base_url;
        current_settings.api_key = config_manager_->get_config().api.api_key;
        current_settings.api_model = config_manager_->get_config().api.model;
        current_settings.whisper_model_path = config_manager_->get_config().whisper.model_path;
        current_settings.use_gpu = config_manager_->get_config().whisper.use_gpu;
        settings_window_->set_settings(current_settings);

        // Set up save callback
        settings_window_->set_save_callback([this](const SettingsData& settings) {
            this->save_settings(settings);
        });

        // Set up D-Bus callbacks for tray icon
        dbus_service_->set_show_window_callback([this]() {
            this->recording_window_->show();
        });

        dbus_service_->set_show_settings_callback([this]() {
            this->show_settings();
        });

        dbus_service_->set_quit_callback([this]() {
            this->quit();
        });

        // Launch tray icon helper process
        launch_tray_helper();

        // Start audio level updates
        g_timeout_add(100, [](gpointer data) -> gboolean {
            auto* daemon = static_cast<SasayakuDaemon*>(data);
            if (daemon->recording_coordinator_->get_state() == RecordingState::RECORDING) {
                float level = daemon->recording_coordinator_->get_audio_level();
                daemon->recording_window_->set_audio_level(level);
            }
            return G_SOURCE_CONTINUE;
        }, this);

        // Show the window
        recording_window_->show();

        std::cout << "Sasayaku started successfully!" << std::endl;
        std::cout << "Click 'Start Recording' or use 'sasayaku-cli toggle-recording'" << std::endl;

        return true;
    }

    void run() {
        std::cout << "Starting main loop..." << std::endl;

        // Use GLib main loop for GTK
        main_loop_ = g_main_loop_new(nullptr, FALSE);

        std::cout << "Main loop created, entering event loop..." << std::endl;

        // Run the main loop - this blocks until quit() is called
        g_main_loop_run(main_loop_);

        std::cout << "Main loop exited" << std::endl;

        // Clean up tray helper
        if (tray_pid_ > 0) {
            kill(tray_pid_, SIGTERM);
            waitpid(tray_pid_, nullptr, 0);
        }

        g_main_loop_unref(main_loop_);
    }

private:
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<ModeManager> mode_manager_;
    std::unique_ptr<RecordingCoordinator> recording_coordinator_;
    std::unique_ptr<WindowTracker> window_tracker_;
    std::unique_ptr<DBusService> dbus_service_;
    std::unique_ptr<RecordingWindow> recording_window_;
    std::unique_ptr<SettingsWindow> settings_window_;
    pid_t tray_pid_ = -1;
    GMainLoop* main_loop_ = nullptr;

    void launch_tray_helper() {
        tray_pid_ = fork();
        if (tray_pid_ == 0) {
            // Child process
            execl("./sasayaku-tray", "sasayaku-tray", nullptr);
            // If exec fails, try installed path
            execl("/usr/local/bin/sasayaku-tray", "sasayaku-tray", nullptr);
            std::cerr << "Failed to launch tray helper" << std::endl;
            exit(1);
        } else if (tray_pid_ > 0) {
            std::cout << "Launched tray helper (PID: " << tray_pid_ << ")" << std::endl;
        } else {
            std::cerr << "Failed to fork tray helper" << std::endl;
        }
    }

    void start_recording() {
        // Check if we need to switch modes based on active app
        std::string active_app = window_tracker_->get_active_app_id();
        if (!active_app.empty()) {
            std::string mode_for_app = mode_manager_->get_mode_for_app(active_app);
            if (mode_for_app != mode_manager_->get_current_mode()) {
                std::cout << "Auto-switching to mode: " << mode_for_app
                          << " for app: " << active_app << std::endl;
                mode_manager_->set_current_mode(mode_for_app);
            }
        }

        if (recording_coordinator_->start_recording()) {
            std::cout << "Recording started" << std::endl;
            recording_window_->set_recording_state(true);
            recording_window_->set_status_text("Recording... Speak now");
            recording_window_->set_transcription_text("");
            dbus_service_->emit_recording_started();
        } else {
            std::cerr << "Failed to start recording" << std::endl;
            recording_window_->set_status_text("Error: Failed to start recording");
        }
    }

    void stop_recording() {
        std::cout << "Stopping recording..." << std::endl;
        recording_window_->set_status_text("Processing transcription...");

        recording_coordinator_->stop_recording([this](const std::string& text, bool success) {
            this->recording_window_->set_recording_state(false);

            if (success && !text.empty()) {
                std::cout << "Final text: " << text << std::endl;
                this->recording_window_->set_transcription_text(text);
                this->recording_window_->set_status_text("✓ Transcription complete - Copied to clipboard");
                this->dbus_service_->emit_transcription_complete(text);
            } else {
                this->recording_window_->set_status_text("✗ Transcription failed");
            }

            this->dbus_service_->emit_recording_stopped();
        });
    }

    void toggle_recording() {
        if (recording_coordinator_->get_state() == RecordingState::RECORDING) {
            stop_recording();
        } else {
            start_recording();
        }
    }

    std::string get_status() const {
        switch (recording_coordinator_->get_state()) {
            case RecordingState::RECORDING:
                return "recording";
            case RecordingState::PROCESSING:
                return "processing";
            case RecordingState::STOPPED:
                return "stopped";
            case RecordingState::ERROR:
                return "error";
            default:
                return "unknown";
        }
    }

    void show_settings() {
        if (settings_window_) {
            settings_window_->show();
        }
    }

    void save_settings(const SettingsData& settings) {
        std::cout << "Saving settings..." << std::endl;

        // Update config
        auto& config = config_manager_->get_mutable_config();
        config.api.base_url = settings.api_base_url;
        config.api.api_key = settings.api_key;
        config.api.model = settings.api_model;
        config.whisper.model_path = settings.whisper_model_path;
        config.whisper.use_gpu = settings.use_gpu;

        // Save to file
        if (config_manager_->save()) {
            std::cout << "Settings saved successfully to: " << config_manager_->get_config_path() << std::endl;

            // Reinitialize components that depend on config
            mode_manager_->initialize(config_manager_.get());
            recording_coordinator_->initialize(config_manager_.get(), mode_manager_.get());

            std::cout << "Configuration reloaded" << std::endl;
        } else {
            std::cerr << "Failed to save settings" << std::endl;
        }
    }

    void quit() {
        std::cout << "Quitting..." << std::endl;
        if (main_loop_) {
            g_main_loop_quit(main_loop_);
        }
    }
};

int main(int argc, char** argv) {
    std::cout << "Sasayaku daemon starting..." << std::endl;

    SasayakuDaemon daemon;

    std::cout << "Initializing daemon..." << std::endl;
    if (!daemon.initialize(argc, argv)) {
        std::cerr << "Failed to initialize daemon" << std::endl;
        return 1;
    }

    std::cout << "Initialization complete, running..." << std::endl;
    daemon.run();

    std::cout << "Daemon exiting" << std::endl;
    return 0;
}
