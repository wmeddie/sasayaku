#pragma once

#include <gio/gio.h>
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <utility>

namespace sasayaku {

// D-Bus interface callbacks
using RecordingCommandCallback = std::function<void()>;
using StatusCallback = std::function<std::string()>;

class DBusService {
public:
    DBusService();
    ~DBusService();

    // Initialize and register D-Bus service
    bool initialize();

    // Set callbacks
    void set_start_recording_callback(RecordingCommandCallback cb) {
        start_recording_cb_ = cb;
    }

    void set_stop_recording_callback(RecordingCommandCallback cb) {
        stop_recording_cb_ = cb;
    }

    void set_toggle_recording_callback(RecordingCommandCallback cb) {
        toggle_recording_cb_ = cb;
    }

    void set_status_callback(StatusCallback cb) {
        status_cb_ = cb;
    }

    void set_quit_callback(std::function<void()> cb) {
        quit_callback_ = cb;
    }

    void set_get_modes_callback(
        std::function<std::vector<std::pair<std::string, std::string>>()> cb) {
        get_modes_cb_ = cb;
    }

    void set_get_current_mode_callback(std::function<std::string()> cb) {
        get_current_mode_cb_ = cb;
    }

    void set_set_mode_callback(std::function<void(const std::string&)> cb) {
        set_mode_cb_ = cb;
    }

    // Run the main loop (blocking)
    void run();

    // Quit the main loop
    void quit();

    // Emit signals
    void emit_recording_started();
    void emit_recording_stopped();
    void emit_transcription_complete(const std::string& text);
    void emit_state_changed(const std::string& state);
    void emit_audio_level(double level);
    void emit_error(const std::string& message);

    // Public static callbacks for GDBus
    static void on_bus_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_lost(GDBusConnection* connection, const gchar* name, gpointer user_data);

    static void handle_method_call(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* object_path,
        const gchar* interface_name,
        const gchar* method_name,
        GVariant* parameters,
        GDBusMethodInvocation* invocation,
        gpointer user_data
    );

private:
    GDBusConnection* connection_ = nullptr;
    guint owner_id_ = 0;
    guint registration_id_ = 0;

    RecordingCommandCallback start_recording_cb_;
    RecordingCommandCallback stop_recording_cb_;
    RecordingCommandCallback toggle_recording_cb_;
    StatusCallback status_cb_;
    std::function<void()> quit_callback_;
    std::function<std::vector<std::pair<std::string, std::string>>()> get_modes_cb_;
    std::function<std::string()> get_current_mode_cb_;
    std::function<void(const std::string&)> set_mode_cb_;

    void register_object();
};

} // namespace sasayaku
