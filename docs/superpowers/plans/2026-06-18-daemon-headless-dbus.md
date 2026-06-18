# Headless Daemon + HUD D-Bus Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn `sasayaku-daemon` into a headless service (no GTK4 HUD) that exposes the D-Bus surface the GNOME extension's HUD will drive — recording control, mode listing/selection, and `StateChanged`/`AudioLevel`/`TranscriptionComplete`/`Error` signals.

**Architecture:** The daemon already runs a GLib main loop with a GDBus service (`org.sasayaku.Daemon`). We strip the GTK4 windows (`recording_window`, `settings_window`) and the tray-launch from `main.cpp`, keep the GLib loop + coordinator/mode-manager/config wiring, extend the D-Bus interface, and emit lifecycle/audio signals at the existing transition points. A small pure-logic helper (`recording_state_to_string`) gets the repo's first unit test.

**Tech Stack:** C++17, GLib/GIO (GDBus), Meson, `gdbus`/`dbus-monitor` for integration verification.

## Global Constraints

- D-Bus name `org.sasayaku.Daemon`, object path `/org/sasayaku/Daemon`, interface `org.sasayaku.Daemon` (copied verbatim from existing code).
- Keep existing methods working for `sasayaku-cli`: `StartRecording`, `StopRecording`, `ToggleRecording`, `GetStatus() → s`, `Quit`. `GetStatus` keeps returning the coordinator state strings `stopped`/`recording`/`processing`/`error`.
- Keep existing signals `RecordingStarted`, `RecordingStopped`, `TranscriptionComplete(s)`.
- HUD state strings used by `StateChanged`: `idle` / `recording` / `processing` / `done` / `error` (spec §5.2, §6).
- The daemon must remain driveable entirely over D-Bus (no GUI). It keeps a GLib main loop (GDBus needs it).
- Residual `gtk4` link is allowed in this phase (clipboard); `libadwaita` is removed. Full GTK4 removal is out of scope (deferred).
- GPL-3.0 repo. Commit messages end with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
- Build with `meson compile -C build`; whisper.cpp already built (Vulkan) under `whisper.cpp/build`.

## Deferred (NOT in this plan — tracked for the prefs/extension phases)

- D-Bus methods `GetConfig`/`SetConfig`/`ListInputDevices`/`ListWhisperModels`/`DownloadModel` + signal `ModelDownloadProgress`, and `ProcessText`. These support the extension `prefs.js` (spec Phase 6) and will land with it.
- Full removal of the `gtk4` dependency (requires moving clipboard/Super-mode off GDK).
- Deleting the now-unbuilt `src/ui/recording_window.*` and `src/ui/settings_window.*` (settings_window holds the HF model-download logic to be ported in the prefs plan).

---

## File Structure

- **Create:** `src/daemon/dbus_helpers.hpp` / `dbus_helpers.cpp` — pure helper `recording_state_to_string(RecordingState) -> std::string`.
- **Create:** `src/daemon/dbus_helpers_test.cpp` — unit test (co-located with the module).
- **Modify:** `src/daemon/dbus_service.hpp` — new callback setters + emit declarations; drop `show_window`/`show_settings`.
- **Modify:** `src/daemon/dbus_service.cpp` — introspection XML + handlers for `GetModes`/`GetCurrentMode`/`SetMode`; emit `StateChanged`/`AudioLevel`/`Error`; drop `ShowWindow`/`ShowSettings`.
- **Modify:** `src/daemon/main.cpp` — headless rewrite (no GTK windows/tray; wire new callbacks; emit signals).
- **Modify:** `src/meson.build` — drop the UI library + `libadwaita` from the daemon, add `dbus_helpers.cpp`, register the unit test.
- **Modify:** `meson.build` — remove the now-unused `libadwaita-1` project dependency.

---

## Task 1: `recording_state_to_string` helper + first unit test

**Files:**
- Create: `src/daemon/dbus_helpers.hpp`, `src/daemon/dbus_helpers.cpp`, `src/daemon/dbus_helpers_test.cpp`
- Modify: `src/meson.build` (add the test target)

**Interfaces:**
- Consumes: `sasayaku::RecordingState` (enum in `src/core/common.hpp`: `STOPPED, RECORDING, PROCESSING, ERROR`).
- Produces: `std::string sasayaku::recording_state_to_string(RecordingState)` — returns `"stopped"`, `"recording"`, `"processing"`, `"error"` (used by `GetStatus` in Task 3).

- [ ] **Step 1: Write the failing test**

Create `src/daemon/dbus_helpers_test.cpp`:
```cpp
#include "dbus_helpers.hpp"
#include "../core/common.hpp"
#include <cassert>
#include <iostream>

using namespace sasayaku;

int main() {
    assert(recording_state_to_string(RecordingState::STOPPED) == "stopped");
    assert(recording_state_to_string(RecordingState::RECORDING) == "recording");
    assert(recording_state_to_string(RecordingState::PROCESSING) == "processing");
    assert(recording_state_to_string(RecordingState::ERROR) == "error");
    std::cout << "dbus_helpers tests passed" << std::endl;
    return 0;
}
```

- [ ] **Step 2: Create the header (declaration only) so the test compiles but fails to link**

Create `src/daemon/dbus_helpers.hpp`:
```cpp
#pragma once

#include "../core/common.hpp"
#include <string>

namespace sasayaku {

// Map the coordinator's recording state to the GetStatus D-Bus string.
std::string recording_state_to_string(RecordingState state);

} // namespace sasayaku
```

Register the test in `src/meson.build` — add at the end of the file:
```meson
# Unit tests
test_dbus_helpers = executable('test-dbus-helpers',
  ['daemon/dbus_helpers.cpp', 'daemon/dbus_helpers_test.cpp'],
  install: false,
)
test('dbus_helpers', test_dbus_helpers)
```

- [ ] **Step 3: Run the test target to verify it fails**

Run: `meson setup build --reconfigure >/dev/null && meson compile -C build test-dbus-helpers`
Expected: FAIL — link error `undefined reference to 'sasayaku::recording_state_to_string'` (there is no `dbus_helpers.cpp` yet).

- [ ] **Step 4: Write the implementation**

Create `src/daemon/dbus_helpers.cpp`:
```cpp
#include "dbus_helpers.hpp"

namespace sasayaku {

std::string recording_state_to_string(RecordingState state) {
    switch (state) {
        case RecordingState::RECORDING:  return "recording";
        case RecordingState::PROCESSING: return "processing";
        case RecordingState::STOPPED:    return "stopped";
        case RecordingState::ERROR:      return "error";
    }
    return "unknown";
}

} // namespace sasayaku
```

- [ ] **Step 5: Build and run the test to verify it passes**

Run: `meson compile -C build test-dbus-helpers && meson test -C build dbus_helpers -v`
Expected: PASS — output contains `dbus_helpers tests passed` and meson reports `1/1` ok.

- [ ] **Step 6: Commit**

```bash
git add src/daemon/dbus_helpers.hpp src/daemon/dbus_helpers.cpp src/daemon/dbus_helpers_test.cpp src/meson.build
git commit -m "$(cat <<'EOF'
Add recording_state_to_string helper + first unit test

Pure mapping from RecordingState to the GetStatus D-Bus string, with a
minimal meson test target (the repo's first unit test).

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Extend the D-Bus interface (methods + signals)

**Files:**
- Modify: `src/daemon/dbus_service.hpp`
- Modify: `src/daemon/dbus_service.cpp`

**Interfaces:**
- Consumes: nothing from Task 1 directly.
- Produces (used by Task 3):
  - `void DBusService::set_get_modes_callback(std::function<std::vector<std::pair<std::string,std::string>>()>)`
  - `void DBusService::set_get_current_mode_callback(std::function<std::string()>)`
  - `void DBusService::set_set_mode_callback(std::function<void(const std::string&)>)`
  - `void DBusService::emit_state_changed(const std::string& state)`
  - `void DBusService::emit_audio_level(double level)`
  - `void DBusService::emit_error(const std::string& message)`
  - New D-Bus methods `GetModes() -> a(ss)`, `GetCurrentMode() -> s`, `SetMode(s)`; new signals `StateChanged(s)`, `AudioLevel(d)`, `Error(s)`. `ShowWindow`/`ShowSettings` removed.

- [ ] **Step 1: Update `dbus_service.hpp` — add includes, callbacks, emit decls; remove window/settings callbacks**

In `src/daemon/dbus_service.hpp`, replace the includes block:
```cpp
#include <gio/gio.h>
#include <functional>
#include <string>
#include <memory>
```
with:
```cpp
#include <gio/gio.h>
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <utility>
```

Replace these three setters:
```cpp
    void set_show_window_callback(std::function<void()> cb) {
        show_window_callback_ = cb;
    }

    void set_show_settings_callback(std::function<void()> cb) {
        show_settings_callback_ = cb;
    }

    void set_quit_callback(std::function<void()> cb) {
        quit_callback_ = cb;
    }
```
with:
```cpp
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
```

Replace the emit declarations:
```cpp
    // Emit signals
    void emit_recording_started();
    void emit_recording_stopped();
    void emit_transcription_complete(const std::string& text);
```
with:
```cpp
    // Emit signals
    void emit_recording_started();
    void emit_recording_stopped();
    void emit_transcription_complete(const std::string& text);
    void emit_state_changed(const std::string& state);
    void emit_audio_level(double level);
    void emit_error(const std::string& message);
```

Replace the private member callbacks:
```cpp
    std::function<void()> show_window_callback_;
    std::function<void()> show_settings_callback_;
    std::function<void()> quit_callback_;
```
with:
```cpp
    std::function<void()> quit_callback_;
    std::function<std::vector<std::pair<std::string, std::string>>()> get_modes_cb_;
    std::function<std::string()> get_current_mode_cb_;
    std::function<void(const std::string&)> set_mode_cb_;
```

- [ ] **Step 2: Update the introspection XML in `dbus_service.cpp`**

In `src/daemon/dbus_service.cpp`, replace the whole `DBUS_INTROSPECTION_XML` definition:
```cpp
static const char* DBUS_INTROSPECTION_XML =
    "<node>"
    "  <interface name='org.sasayaku.Daemon'>"
    "    <method name='StartRecording'/>"
    "    <method name='StopRecording'/>"
    "    <method name='ToggleRecording'/>"
    "    <method name='ShowWindow'/>"
    "    <method name='ShowSettings'/>"
    "    <method name='Quit'/>"
    "    <method name='GetStatus'>"
    "      <arg type='s' name='status' direction='out'/>"
    "    </method>"
    "    <signal name='RecordingStarted'/>"
    "    <signal name='RecordingStopped'/>"
    "    <signal name='TranscriptionComplete'>"
    "      <arg type='s' name='text'/>"
    "    </signal>"
    "  </interface>"
    "</node>";
```
with:
```cpp
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
```

- [ ] **Step 3: Replace the `ShowWindow`/`ShowSettings` handlers with the new method handlers**

In `src/daemon/dbus_service.cpp`, inside `handle_method_call`, replace these two blocks:
```cpp
    else if (g_strcmp0(method_name, "ShowWindow") == 0) {
        if (service->show_window_callback_) {
            service->show_window_callback_();
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
    else if (g_strcmp0(method_name, "ShowSettings") == 0) {
        if (service->show_settings_callback_) {
            service->show_settings_callback_();
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
```
with:
```cpp
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
```

- [ ] **Step 4: Add the new emit functions**

In `src/daemon/dbus_service.cpp`, immediately before the closing `} // namespace sasayaku`, add:
```cpp
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
```

- [ ] **Step 5: Build (daemon will still reference removed callbacks in main.cpp — expect that; build the service objects only)**

Run: `meson compile -C build 2>&1 | grep -iE "dbus_service|error:" | head`
Expected: `dbus_service.cpp` compiles cleanly. `main.cpp` will FAIL to compile because it still calls `set_show_window_callback`/`set_show_settings_callback` — that is fixed in Task 3. Confirm the only errors are in `main.cpp` (e.g. `'set_show_window_callback' is not a member`).

- [ ] **Step 6: Commit**

```bash
git add src/daemon/dbus_service.hpp src/daemon/dbus_service.cpp
git commit -m "$(cat <<'EOF'
Expand org.sasayaku.Daemon D-Bus interface

Add GetModes/GetCurrentMode/SetMode methods and StateChanged/AudioLevel/
Error signals; remove ShowWindow/ShowSettings (UI moves to the GNOME
extension). main.cpp wiring follows.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Headless daemon (`main.cpp` rewrite) + build cleanup

**Files:**
- Modify: `src/daemon/main.cpp` (full rewrite)
- Modify: `src/meson.build` (daemon target: drop UI lib + libadwaita, add `dbus_helpers.cpp`)
- Modify: `meson.build` (remove `libadwaita-1` dependency)

**Interfaces:**
- Consumes: Task 1 `recording_state_to_string`; Task 2 callbacks/emitters.
- Produces: a headless `sasayaku-daemon` binary driven only over D-Bus.

- [ ] **Step 1: Replace `src/daemon/main.cpp` entirely**

Overwrite `src/daemon/main.cpp` with:
```cpp
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
```

- [ ] **Step 2: Update `src/meson.build` — drop the UI library + libadwaita from the daemon, add `dbus_helpers.cpp`**

In `src/meson.build`, replace the UI library block:
```meson
# UI sources
ui_sources = files(
  'ui/recording_window.cpp',
  'ui/settings_window.cpp',
)

# UI library (used by daemon)
libsasayaku_ui = static_library('sasayaku_ui',
  ui_sources,
  dependencies: [gtk_dep, libadwaita_dep, gio_dep, curl_dep],
  install: false,
)

# Daemon sources
daemon_sources = files(
  'daemon/main.cpp',
  'daemon/dbus_service.cpp',
  'daemon/mode_manager.cpp',
  'daemon/window_tracker.cpp',
  'daemon/recording_coordinator.cpp',
)

# Daemon executable
sasayaku_daemon = executable('sasayaku-daemon',
  daemon_sources,
  link_with: [libsasayaku_core, libsasayaku_ai, libsasayaku_utils, libsasayaku_ui],
  dependencies: [gtk_dep, libadwaita_dep, gio_dep, threads_dep, whisper_lib],
  include_directories: [whisper_include],
  install: true,
)
```
with:
```meson
# Daemon sources (headless: the GNOME extension provides the UI)
daemon_sources = files(
  'daemon/main.cpp',
  'daemon/dbus_service.cpp',
  'daemon/dbus_helpers.cpp',
  'daemon/mode_manager.cpp',
  'daemon/window_tracker.cpp',
  'daemon/recording_coordinator.cpp',
)

# Daemon executable. Still links gtk_dep transitively via libsasayaku_utils
# (clipboard uses GdkClipboard); libadwaita is no longer needed.
sasayaku_daemon = executable('sasayaku-daemon',
  daemon_sources,
  link_with: [libsasayaku_core, libsasayaku_ai, libsasayaku_utils],
  dependencies: [gtk_dep, gio_dep, threads_dep, whisper_lib],
  include_directories: [whisper_include],
  install: true,
)
```

(The `test-dbus-helpers` target added in Task 1 stays at the end of the file; `dbus_helpers.cpp` is compiled both there and into the daemon — acceptable.)

- [ ] **Step 3: Remove the unused `libadwaita-1` dependency from the top-level `meson.build`**

In `meson.build`, delete this line:
```meson
libadwaita_dep = dependency('libadwaita-1', version: '>= 1.0')
```
and in the `summary({...}, section: 'Dependencies')` call delete the line:
```meson
    'libadwaita': libadwaita_dep.found(),
```

- [ ] **Step 4: Build the whole project**

Run: `meson setup build --reconfigure >/dev/null && meson compile -C build 2>&1 | tail -5; echo "exit: ${PIPESTATUS[0]}"`
Expected: `exit: 0`; `sasayaku-daemon`, `sasayaku-cli`, and `test-dbus-helpers` all build.

- [ ] **Step 5: Verify the daemon no longer links libadwaita and is headless**

Run: `ldd build/src/sasayaku-daemon | grep -E "libadwaita|libgtk-4" ; echo "---"`
Expected: **no `libadwaita`** line. (`libgtk-4` MAY still appear — it is the documented residual clipboard dependency for this phase.)

- [ ] **Step 6: Integration-verify the new D-Bus surface**

Run (starts daemon in background, queries it, stops it):
```bash
./build/src/sasayaku-daemon >/tmp/sasa-daemon.log 2>&1 &
DPID=$!
sleep 2
echo "== introspect =="; gdbus introspect --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon | grep -E "GetModes|GetCurrentMode|SetMode|StateChanged|AudioLevel|Error|ShowWindow"
echo "== GetModes =="; gdbus call --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon --method org.sasayaku.Daemon.GetModes
echo "== GetCurrentMode =="; gdbus call --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon --method org.sasayaku.Daemon.GetCurrentMode
echo "== SetMode email then GetCurrentMode =="; gdbus call --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon --method org.sasayaku.Daemon.SetMode "email"; gdbus call --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon --method org.sasayaku.Daemon.GetCurrentMode
kill $DPID 2>/dev/null
```
Expected: introspect lists `GetModes`/`GetCurrentMode`/`SetMode`/`StateChanged`/`AudioLevel`/`Error` and shows **no** `ShowWindow`. `GetModes` returns a non-empty `a(ss)` like `([('voice_to_text', 'Voice to Text'), ('email', 'Email'), ...],)`. After `SetMode "email"`, `GetCurrentMode` returns `('email',)`. (Use a mode id that exists in your config; `voice_to_text` and `email` are defaults.)

- [ ] **Step 7: Verify `StateChanged` fires on recording start**

Run:
```bash
./build/src/sasayaku-daemon >/tmp/sasa-daemon.log 2>&1 &
DPID=$!
sleep 2
( dbus-monitor --session "interface='org.sasayaku.Daemon'" >/tmp/sasa-signals.log 2>&1 & echo $! >/tmp/sasa-mon.pid )
sleep 1
gdbus call --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon --method org.sasayaku.Daemon.StartRecording
sleep 1
gdbus call --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon --method org.sasayaku.Daemon.StopRecording
sleep 1
kill "$(cat /tmp/sasa-mon.pid)" $DPID 2>/dev/null
echo "== signals seen =="; grep -E "StateChanged|RecordingStarted|AudioLevel" /tmp/sasa-signals.log | head
```
Expected: the log shows `RecordingStarted`, a `StateChanged` with string `recording`, and at least one `AudioLevel` (a double) while recording. (Transcription of silence may yield an `Error`/`idle` afterward — that is fine; we are only verifying the recording-start signal path.)

- [ ] **Step 8: Commit**

```bash
git add src/daemon/main.cpp src/meson.build meson.build
git commit -m "$(cat <<'EOF'
Make the daemon headless

Drop the GTK4 HUD windows and tray launch from main.cpp; the GNOME Shell
extension now owns the UI. The daemon keeps its GLib loop + D-Bus service,
wires GetModes/GetCurrentMode/SetMode, and emits StateChanged/AudioLevel/
Error at the recording lifecycle transitions. Remove the UI static library
and the libadwaita dependency (gtk4 remains only for clipboard).

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

**1. Spec coverage (against `2026-06-18-linux-gnome-accessibility-hud-design.md`):**
- §5.1 "Remove the GTK4 HUD from the GNOME path; daemon headless" → Task 3 (main.cpp rewrite, drop UI lib). Partial on "no longer links GTK4/libadwaita": libadwaita removed; gtk4 residual is documented and deferred (Deferred section).
- §6 methods `GetModes`/`GetCurrentMode`/`SetMode` → Task 2/3. Signals `StateChanged`/`AudioLevel`/`Error` → Task 2/3. `TranscriptionComplete` retained. `ShowWindow`/`ShowSettings` removed → Task 2.
- §6 config/model/device methods + `ProcessText` + `ModelDownloadProgress` → explicitly Deferred (paired with prefs phase).
- §7 data-flow signals (`recording`→`processing`→`done`) → emitted in Task 3 `start_recording`/`stop_recording`.

**2. Placeholder scan:** No TBD/TODO; every code step has complete code; every verification step has an exact command and expected output. None of the forbidden patterns present.

**3. Type consistency:** `recording_state_to_string(RecordingState)→std::string` defined in Task 1, used in Task 3 `get_status`. Callback signatures in Task 2 (`set_get_modes_callback` taking `std::function<std::vector<std::pair<std::string,std::string>>()>`, `set_set_mode_callback` taking `std::function<void(const std::string&)>`) match the lambdas wired in Task 3. Emit names `emit_state_changed`/`emit_audio_level`/`emit_error` consistent between Task 2 (decl/def) and Task 3 (calls). D-Bus method/signal names consistent between the introspection XML, handlers, and the gdbus verification commands.
