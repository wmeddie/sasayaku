#include <gio/gio.h>
#include <iostream>

int main() {
    GError* error = nullptr;

    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (error) {
        std::cerr << "Error connecting to D-Bus: " << error->message << std::endl;
        g_error_free(error);
        return 1;
    }

    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "org.sasayaku.Daemon",
        "/org/sasayaku/Daemon",
        "org.sasayaku.Daemon",
        "ToggleRecording",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (error) {
        std::cerr << "Error: " << error->message << std::endl;
        std::cerr << "Make sure sasayaku-daemon is running." << std::endl;
        g_error_free(error);
        g_object_unref(connection);
        return 1;
    }

    if (result) {
        g_variant_unref(result);
    }

    g_object_unref(connection);
    return 0;
}
