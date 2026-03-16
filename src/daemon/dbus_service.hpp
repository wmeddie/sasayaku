#pragma once

#include <gio/gio.h>
#include <functional>
#include <string>
#include <memory>

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

    void set_show_window_callback(std::function<void()> cb) {
        show_window_callback_ = cb;
    }

    void set_show_settings_callback(std::function<void()> cb) {
        show_settings_callback_ = cb;
    }

    void set_quit_callback(std::function<void()> cb) {
        quit_callback_ = cb;
    }

    // Run the main loop (blocking)
    void run();

    // Quit the main loop
    void quit();

    // Emit signals
    void emit_recording_started();
    void emit_recording_stopped();
    void emit_transcription_complete(const std::string& text);

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
    std::function<void()> show_window_callback_;
    std::function<void()> show_settings_callback_;
    std::function<void()> quit_callback_;

    void register_object();
};

} // namespace sasayaku
