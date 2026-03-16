#include <gio/gio.h>
#include <iostream>
#include <cstring>

static void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <command>" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  start-recording    Start recording audio" << std::endl;
    std::cout << "  stop-recording     Stop recording and transcribe" << std::endl;
    std::cout << "  toggle-recording   Toggle recording on/off" << std::endl;
    std::cout << "  status             Get current recording status" << std::endl;
    std::cout << "  help               Show this help message" << std::endl;
}

static bool call_dbus_method(const char* method_name) {
    GError* error = nullptr;

    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (error) {
        std::cerr << "Error connecting to D-Bus: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "org.sasayaku.Daemon",
        "/org/sasayaku/Daemon",
        "org.sasayaku.Daemon",
        method_name,
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (error) {
        std::cerr << "Error calling D-Bus method '" << method_name << "': " << error->message << std::endl;
        std::cerr << "Make sure sasayaku-daemon is running." << std::endl;
        g_error_free(error);
        g_object_unref(connection);
        return false;
    }

    if (result) {
        g_variant_unref(result);
    }

    g_object_unref(connection);
    return true;
}

static bool get_status() {
    GError* error = nullptr;

    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (error) {
        std::cerr << "Error connecting to D-Bus: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "org.sasayaku.Daemon",
        "/org/sasayaku/Daemon",
        "org.sasayaku.Daemon",
        "GetStatus",
        nullptr,
        G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (error) {
        std::cerr << "Error getting status: " << error->message << std::endl;
        std::cerr << "Make sure sasayaku-daemon is running." << std::endl;
        g_error_free(error);
        g_object_unref(connection);
        return false;
    }

    if (result) {
        const gchar* status = nullptr;
        g_variant_get(result, "(&s)", &status);
        std::cout << "Status: " << status << std::endl;
        g_variant_unref(result);
    }

    g_object_unref(connection);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* command = argv[1];

    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(command, "start-recording") == 0) {
        if (!call_dbus_method("StartRecording")) {
            return 1;
        }
        std::cout << "Recording started" << std::endl;
        return 0;
    }

    if (strcmp(command, "stop-recording") == 0) {
        if (!call_dbus_method("StopRecording")) {
            return 1;
        }
        std::cout << "Recording stopped" << std::endl;
        return 0;
    }

    if (strcmp(command, "toggle-recording") == 0) {
        if (!call_dbus_method("ToggleRecording")) {
            return 1;
        }
        std::cout << "Recording toggled" << std::endl;
        return 0;
    }

    if (strcmp(command, "status") == 0) {
        if (!get_status()) {
            return 1;
        }
        return 0;
    }

    std::cerr << "Unknown command: " << command << std::endl;
    print_usage(argv[0]);
    return 1;
}
