#include "dbus_service.hpp"
#include "dbus_helpers.hpp"
#include "mode_manager.hpp"
#include "window_tracker.hpp"
#include "recording_coordinator.hpp"
#include "../utils/config_manager.hpp"
#include <glib.h>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace sasayaku;

// Headless daemon: all UI is provided by the GNOME Shell extension over D-Bus.
class SasayakuDaemon {
public:
    SasayakuDaemon() = default;

    bool initialize() {
        config_manager_ = std::make_unique<ConfigManager>();
        if (!config_manager_->load()) {
            std::cout << "Creating default configuration..." << std::endl;
            config_manager_->initialize_defaults();
            auto& config = config_manager_->get_mutable_config();
            config.whisper.model_path =
                config_manager_->get_data_dir() + "/models/ggml-large-v3-turbo.bin";
            config_manager_->save();
        }
        std::cout << "Configuration loaded from: " << config_manager_->get_config_path() << std::endl;

        if (config_manager_->get_config().whisper.model_path.empty()) {
            std::cerr << "Error: Whisper model path not configured!" << std::endl;
            return false;
        }

        mode_manager_ = std::make_unique<ModeManager>();
        mode_manager_->initialize(config_manager_.get());

        recording_coordinator_ = std::make_unique<RecordingCoordinator>();
        if (!recording_coordinator_->initialize(config_manager_.get(), mode_manager_.get())) {
            std::cerr << "Failed to initialize recording coordinator" << std::endl;
            return false;
        }

        window_tracker_ = std::make_unique<WindowTracker>();
        window_tracker_->initialize();

        dbus_service_ = std::make_unique<DBusService>();
        dbus_service_->set_start_recording_callback([this]() { this->start_recording(); });
        dbus_service_->set_stop_recording_callback([this]() { this->stop_recording(); });
        dbus_service_->set_toggle_recording_callback([this]() { this->toggle_recording(); });
        dbus_service_->set_status_callback([this]() { return this->get_status(); });
        dbus_service_->set_get_modes_callback([this]() { return this->get_modes(); });
        dbus_service_->set_get_current_mode_callback(
            [this]() { return mode_manager_->get_current_mode(); });
        dbus_service_->set_set_mode_callback(
            [this](const std::string& id) { mode_manager_->set_current_mode(id); });
        dbus_service_->set_quit_callback([this]() { this->quit(); });

        if (!dbus_service_->initialize()) {
            std::cerr << "Failed to initialize D-Bus service" << std::endl;
            return false;
        }

        // Emit AudioLevel while recording (100ms cadence) for the HUD waveform.
        g_timeout_add(100, &SasayakuDaemon::on_level_timer, this);

        std::cout << "Sasayaku daemon ready (headless). "
                     "Drive via org.sasayaku.Daemon or sasayaku-cli." << std::endl;
        return true;
    }

    void run() {
        main_loop_ = g_main_loop_new(nullptr, FALSE);
        g_main_loop_run(main_loop_);
        g_main_loop_unref(main_loop_);
    }

private:
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<ModeManager> mode_manager_;
    std::unique_ptr<RecordingCoordinator> recording_coordinator_;
    std::unique_ptr<WindowTracker> window_tracker_;
    std::unique_ptr<DBusService> dbus_service_;
    GMainLoop* main_loop_ = nullptr;

    static gboolean on_level_timer(gpointer data) {
        auto* self = static_cast<SasayakuDaemon*>(data);
        if (self->recording_coordinator_->get_state() == RecordingState::RECORDING) {
            self->dbus_service_->emit_audio_level(self->recording_coordinator_->get_audio_level());
        }
        return G_SOURCE_CONTINUE;
    }

    std::vector<std::pair<std::string, std::string>> get_modes() const {
        std::vector<std::pair<std::string, std::string>> result;
        for (const auto& [id, mode] : config_manager_->get_config().modes) {
            result.emplace_back(id, mode.name);
        }
        return result;
    }

    void start_recording() {
        // NOTE: app-based auto mode-switching used to query GNOME Shell here via
        // a blocking `gdbus ... Shell.Eval`. That deadlocks when the caller is the
        // Shell extension (Shell waits on us; we'd wait on Shell) and Eval is
        // disabled on GNOME 50. Active-window detection will move into the
        // extension (which has Meta access) and arrive via SetMode.
        if (recording_coordinator_->start_recording()) {
            std::cout << "Recording started" << std::endl;
            dbus_service_->emit_recording_started();
            dbus_service_->emit_state_changed("recording");
        } else {
            std::cerr << "Failed to start recording" << std::endl;
            dbus_service_->emit_error("Failed to start recording");
            dbus_service_->emit_state_changed("idle");
        }
    }

    void stop_recording() {
        if (recording_coordinator_->get_state() != RecordingState::RECORDING) {
            return;
        }
        std::cout << "Stopping recording..." << std::endl;
        dbus_service_->emit_state_changed("processing");

        recording_coordinator_->stop_recording([this](const std::string& text, bool success) {
            if (success && !text.empty()) {
                std::cout << "Final text: " << text << std::endl;
                dbus_service_->emit_transcription_complete(text);
                dbus_service_->emit_state_changed("done");
            } else {
                dbus_service_->emit_error("Transcription failed");
                dbus_service_->emit_state_changed("idle");
            }
            dbus_service_->emit_recording_stopped();
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
        return recording_state_to_string(recording_coordinator_->get_state());
    }

    void quit() {
        std::cout << "Quitting..." << std::endl;
        if (main_loop_) {
            g_main_loop_quit(main_loop_);
        }
    }
};

int main(int /*argc*/, char** /*argv*/) {
    std::cout << "Sasayaku daemon starting..." << std::endl;

    SasayakuDaemon daemon;
    if (!daemon.initialize()) {
        std::cerr << "Failed to initialize daemon" << std::endl;
        return 1;
    }

    daemon.run();
    std::cout << "Daemon exiting" << std::endl;
    return 0;
}
