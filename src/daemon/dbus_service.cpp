#include "dbus_service.hpp"
#include <iostream>

namespace sasayaku {

static const char* DBUS_INTROSPECTION_XML =
    "<node>"
    "  <interface name='org.sasayaku.Daemon'>"
    "    <method name='StartRecording'/>"
    "    <method name='StopRecording'/>"
    "    <method name='ToggleRecording'/>"
    "    <method name='Quit'/>"
    "    <method name='GetStatus'>"
    "      <arg type='s' name='status' direction='out'/>"
    "    </method>"
    "    <method name='GetModes'>"
    "      <arg type='a(ss)' name='modes' direction='out'/>"
    "    </method>"
    "    <method name='GetCurrentMode'>"
    "      <arg type='s' name='mode_id' direction='out'/>"
    "    </method>"
    "    <method name='SetMode'>"
    "      <arg type='s' name='mode_id' direction='in'/>"
    "    </method>"
    "    <signal name='RecordingStarted'/>"
    "    <signal name='RecordingStopped'/>"
    "    <signal name='TranscriptionComplete'>"
    "      <arg type='s' name='text'/>"
    "    </signal>"
    "    <signal name='StateChanged'>"
    "      <arg type='s' name='state'/>"
    "    </signal>"
    "    <signal name='AudioLevel'>"
    "      <arg type='d' name='level'/>"
    "    </signal>"
    "    <signal name='Error'>"
    "      <arg type='s' name='message'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

static const GDBusInterfaceVTable interface_vtable = {
    DBusService::handle_method_call,
    nullptr,  // get_property
    nullptr   // set_property
};

DBusService::DBusService() {
}

DBusService::~DBusService() {
    if (registration_id_ > 0) {
        g_dbus_connection_unregister_object(connection_, registration_id_);
    }

    if (owner_id_ > 0) {
        g_bus_unown_name(owner_id_);
    }
}

bool DBusService::initialize() {
    owner_id_ = g_bus_own_name(
        G_BUS_TYPE_SESSION,
        "org.sasayaku.Daemon",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        on_bus_acquired,
        on_name_acquired,
        on_name_lost,
        this,
        nullptr
    );

    return owner_id_ > 0;
}

void DBusService::on_bus_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data) {
    auto* service = static_cast<DBusService*>(user_data);
    service->connection_ = connection;
    service->register_object();
    std::cout << "D-Bus acquired: " << name << std::endl;
}

void DBusService::on_name_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data) {
    std::cout << "D-Bus name acquired: " << name << std::endl;
}

void DBusService::on_name_lost(GDBusConnection* connection, const gchar* name, gpointer user_data) {
    std::cerr << "D-Bus name lost: " << name << std::endl;
}

void DBusService::register_object() {
    GError* error = nullptr;

    GDBusNodeInfo* introspection_data = g_dbus_node_info_new_for_xml(DBUS_INTROSPECTION_XML, &error);
    if (error) {
        std::cerr << "Error parsing introspection XML: " << error->message << std::endl;
        g_error_free(error);
        return;
    }

    registration_id_ = g_dbus_connection_register_object(
        connection_,
        "/org/sasayaku/Daemon",
        introspection_data->interfaces[0],
        &interface_vtable,
        this,
        nullptr,
        &error
    );

    if (error) {
        std::cerr << "Error registering object: " << error->message << std::endl;
        g_error_free(error);
    }

    g_dbus_node_info_unref(introspection_data);
}

void DBusService::handle_method_call(
    GDBusConnection* connection,
    const gchar* sender,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* method_name,
    GVariant* parameters,
    GDBusMethodInvocation* invocation,
    gpointer user_data
) {
    auto* service = static_cast<DBusService*>(user_data);

    if (g_strcmp0(method_name, "StartRecording") == 0) {
        if (service->start_recording_cb_) {
            service->start_recording_cb_();
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
    else if (g_strcmp0(method_name, "StopRecording") == 0) {
        if (service->stop_recording_cb_) {
            service->stop_recording_cb_();
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
    else if (g_strcmp0(method_name, "ToggleRecording") == 0) {
        if (service->toggle_recording_cb_) {
            service->toggle_recording_cb_();
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
    else if (g_strcmp0(method_name, "GetModes") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));
        if (service->get_modes_cb_) {
            for (const auto& [id, name] : service->get_modes_cb_()) {
                g_variant_builder_add(&builder, "(ss)", id.c_str(), name.c_str());
            }
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(ss))", &builder));
    }
    else if (g_strcmp0(method_name, "GetCurrentMode") == 0) {
        std::string mode_id;
        if (service->get_current_mode_cb_) {
            mode_id = service->get_current_mode_cb_();
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", mode_id.c_str()));
    }
    else if (g_strcmp0(method_name, "SetMode") == 0) {
        const gchar* mode_id = nullptr;
        g_variant_get(parameters, "(&s)", &mode_id);
        if (service->set_mode_cb_ && mode_id) {
            service->set_mode_cb_(mode_id);
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
    else if (g_strcmp0(method_name, "Quit") == 0) {
        if (service->quit_callback_) {
            service->quit_callback_();
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
    else if (g_strcmp0(method_name, "GetStatus") == 0) {
        std::string status = "stopped";
        if (service->status_cb_) {
            status = service->status_cb_();
        }
        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(s)", status.c_str())
        );
    }
}

void DBusService::emit_recording_started() {
    if (!connection_) return;

    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_,
        nullptr,
        "/org/sasayaku/Daemon",
        "org.sasayaku.Daemon",
        "RecordingStarted",
        nullptr,
        &error
    );

    if (error) {
        std::cerr << "Error emitting signal: " << error->message << std::endl;
        g_error_free(error);
    }
}

void DBusService::emit_recording_stopped() {
    if (!connection_) return;

    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_,
        nullptr,
        "/org/sasayaku/Daemon",
        "org.sasayaku.Daemon",
        "RecordingStopped",
        nullptr,
        &error
    );

    if (error) {
        std::cerr << "Error emitting signal: " << error->message << std::endl;
        g_error_free(error);
    }
}

void DBusService::emit_transcription_complete(const std::string& text) {
    if (!connection_) return;

    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_,
        nullptr,
        "/org/sasayaku/Daemon",
        "org.sasayaku.Daemon",
        "TranscriptionComplete",
        g_variant_new("(s)", text.c_str()),
        &error
    );

    if (error) {
        std::cerr << "Error emitting signal: " << error->message << std::endl;
        g_error_free(error);
    }
}

void DBusService::emit_state_changed(const std::string& state) {
    if (!connection_) return;
    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_, nullptr, "/org/sasayaku/Daemon", "org.sasayaku.Daemon",
        "StateChanged", g_variant_new("(s)", state.c_str()), &error);
    if (error) { std::cerr << "Error emitting StateChanged: " << error->message << std::endl; g_error_free(error); }
}

void DBusService::emit_audio_level(double level) {
    if (!connection_) return;
    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_, nullptr, "/org/sasayaku/Daemon", "org.sasayaku.Daemon",
        "AudioLevel", g_variant_new("(d)", level), &error);
    if (error) { std::cerr << "Error emitting AudioLevel: " << error->message << std::endl; g_error_free(error); }
}

void DBusService::emit_error(const std::string& message) {
    if (!connection_) return;
    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_, nullptr, "/org/sasayaku/Daemon", "org.sasayaku.Daemon",
        "Error", g_variant_new("(s)", message.c_str()), &error);
    if (error) { std::cerr << "Error emitting Error: " << error->message << std::endl; g_error_free(error); }
}

} // namespace sasayaku
