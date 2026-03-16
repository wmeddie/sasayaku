// Separate process for tray icon (uses GTK3)
// Communicates with main daemon via D-Bus

#include <gtk-3.0/gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <gio/gio.h>
#include <iostream>
#include <signal.h>

class TrayHelper {
public:
    TrayHelper() = default;

    bool initialize() {
        // Connect to daemon's D-Bus service
        GError* error = nullptr;
        connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (error) {
            std::cerr << "Failed to connect to D-Bus: " << error->message << std::endl;
            g_error_free(error);
            return false;
        }

        // Subscribe to daemon signals
        g_dbus_connection_signal_subscribe(
            connection_,
            "org.sasayaku.Daemon",
            "org.sasayaku.Daemon",
            "RecordingStarted",
            "/org/sasayaku/Daemon",
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_recording_started,
            this,
            nullptr
        );

        g_dbus_connection_signal_subscribe(
            connection_,
            "org.sasayaku.Daemon",
            "org.sasayaku.Daemon",
            "RecordingStopped",
            "/org/sasayaku/Daemon",
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_recording_stopped,
            this,
            nullptr
        );

        // Create app indicator
        indicator_ = app_indicator_new(
            "sasayaku-indicator",
            "audio-input-microphone-symbolic",
            APP_INDICATOR_CATEGORY_APPLICATION_STATUS
        );

        if (!indicator_) {
            std::cerr << "Failed to create AppIndicator" << std::endl;
            return false;
        }

        app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_ACTIVE);
        app_indicator_set_title(indicator_, "Sasayaku");

        create_menu();

        std::cout << "Tray helper initialized" << std::endl;
        return true;
    }

    void run() {
        gtk_main();
    }

private:
    GDBusConnection* connection_ = nullptr;
    AppIndicator* indicator_ = nullptr;
    GtkWidget* menu_ = nullptr;
    GtkWidget* status_item_ = nullptr;

    void create_menu() {
        menu_ = gtk_menu_new();

        // Status item
        status_item_ = gtk_menu_item_new_with_label("Ready");
        gtk_widget_set_sensitive(status_item_, FALSE);
        gtk_widget_show(status_item_);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), status_item_);

        // Separator
        GtkWidget* sep = gtk_separator_menu_item_new();
        gtk_widget_show(sep);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), sep);

        // Show Window
        GtkWidget* show_item = gtk_menu_item_new_with_label("Show Window");
        gtk_widget_show(show_item);
        g_signal_connect(show_item, "activate", G_CALLBACK(on_show_window), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), show_item);

        // Settings
        GtkWidget* settings_item = gtk_menu_item_new_with_label("Settings");
        gtk_widget_show(settings_item);
        g_signal_connect(settings_item, "activate", G_CALLBACK(on_settings), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), settings_item);

        // Separator
        GtkWidget* sep2 = gtk_separator_menu_item_new();
        gtk_widget_show(sep2);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), sep2);

        // Quit
        GtkWidget* quit_item = gtk_menu_item_new_with_label("Quit");
        gtk_widget_show(quit_item);
        g_signal_connect(quit_item, "activate", G_CALLBACK(on_quit), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), quit_item);

        gtk_widget_show(menu_);
        app_indicator_set_menu(indicator_, GTK_MENU(menu_));
    }

    void set_recording_state(bool recording) {
        if (status_item_) {
            if (recording) {
                gtk_menu_item_set_label(GTK_MENU_ITEM(status_item_), "🔴 Recording...");
                app_indicator_set_icon_full(indicator_, "audio-input-microphone-high-symbolic", "Recording");
            } else {
                gtk_menu_item_set_label(GTK_MENU_ITEM(status_item_), "Ready");
                app_indicator_set_icon_full(indicator_, "audio-input-microphone-symbolic", "Ready");
            }
        }
    }

    void call_daemon_method(const char* method) {
        GError* error = nullptr;
        g_dbus_connection_call_sync(
            connection_,
            "org.sasayaku.Daemon",
            "/org/sasayaku/Daemon",
            "org.sasayaku.Daemon",
            method,
            nullptr,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error
        );

        if (error) {
            std::cerr << "Failed to call " << method << ": " << error->message << std::endl;
            g_error_free(error);
        }
    }

    static void on_show_window(GtkMenuItem* item, gpointer user_data) {
        auto* helper = static_cast<TrayHelper*>(user_data);
        helper->call_daemon_method("ShowWindow");
    }

    static void on_settings(GtkMenuItem* item, gpointer user_data) {
        auto* helper = static_cast<TrayHelper*>(user_data);
        helper->call_daemon_method("ShowSettings");
    }

    static void on_quit(GtkMenuItem* item, gpointer user_data) {
        auto* helper = static_cast<TrayHelper*>(user_data);
        helper->call_daemon_method("Quit");
        gtk_main_quit();
    }

    static void on_recording_started(GDBusConnection* connection,
                                     const gchar* sender_name,
                                     const gchar* object_path,
                                     const gchar* interface_name,
                                     const gchar* signal_name,
                                     GVariant* parameters,
                                     gpointer user_data) {
        auto* helper = static_cast<TrayHelper*>(user_data);
        helper->set_recording_state(true);
    }

    static void on_recording_stopped(GDBusConnection* connection,
                                     const gchar* sender_name,
                                     const gchar* object_path,
                                     const gchar* interface_name,
                                     const gchar* signal_name,
                                     GVariant* parameters,
                                     gpointer user_data) {
        auto* helper = static_cast<TrayHelper*>(user_data);
        helper->set_recording_state(false);
    }
};

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    TrayHelper helper;
    if (!helper.initialize()) {
        std::cerr << "Failed to initialize tray helper" << std::endl;
        return 1;
    }

    helper.run();
    return 0;
}
