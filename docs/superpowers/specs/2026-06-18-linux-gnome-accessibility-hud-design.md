# Linux GNOME Accessibility HUD — Design

- **Date:** 2026-06-18
- **Status:** Draft (awaiting review)
- **Author:** Eduardo Gonzalez (with Claude)

## 1. Context & Problem

Sasayaku's macOS port is the reference experience: a floating, **non-activating**
HUD panel (`OverlayPanel`, an `NSPanel` with `.nonactivatingPanel`) that shows
recording/processing/result states over a shared C++-equivalent core, plus a menu-bar
item. The Linux build currently has only a plain GTK4 window (`recording_window.cpp`)
and an unbuilt GTK3 tray helper. It does not reach macOS parity, and several pieces
needed for the intended use case are missing.

The intended use case is an **accessibility tool**: a user who cannot reliably use a
keyboard drives everything by mouse — record speech, review/lightly edit the
transcription, then copy it or send it (paste + Enter) into whatever application was
active. This requires a HUD that is visible on top **without stealing focus from the
target application**, plus reliable copy/paste/Enter into that target.

On **GNOME Wayland** (the target environment — Ubuntu 26.04, GNOME Shell 50.1), a normal
Wayland client **cannot**: stay always-on-top, be non-activating, query/focus other
windows, or synthesize input into another window. Mutter forbids all of it. The only
component that *can* do these things is code running **inside GNOME Shell** — i.e. a
GNOME Shell extension, which has Mutter/Clutter/St privileges.

The AMD GPU (Radeon RX 9060 XT) also makes CPU whisper slow; whisper.cpp ships a Vulkan
ggml backend and the toolchain (`glslc`, Vulkan 1.4 headers, `libvulkan_radeon`) is present.

## 2. Goals

1. macOS-parity HUD on GNOME Wayland: floating overlay that does not steal focus from
   the target application, with recording → processing → done states and a live waveform.
2. Mouse-only interaction: editable transcription field plus buttons — Select All, Copy,
   Cut, Paste-into-active-window, Press-Enter-in-active-window, Re-record.
3. Reliable paste/Enter into the **previously focused** window.
4. Native top-bar panel indicator (replacing the legacy GTK3 tray) with recording state
   and a menu.
5. Settings (API config, model download, input device, modes, vocabulary) as native
   extension preferences.
6. Vulkan-accelerated whisper on the AMD card, with CPU fallback.

## 3. Non-Goals (explicitly deferred)

- **X11 / xdotool backend** — deferred. We target GNOME Wayland via the extension only.
- **Wayland-without-extension full parity** — without the extension, behavior degrades
  (clipboard works; reliable paste-into-target does not). No ydotool backend is built in
  this round.
- **Custom on-screen keyboard** — text correction relies on the system OSK / the user's
  existing assistive tech. The field is simply editable.
- **macOS/Windows changes** — out of scope.
- **Publishing to extensions.gnome.org** — out of scope (local install for now).

## 4. Key Decisions (from brainstorming)

| Decision | Choice |
|----------|--------|
| Display server strategy | GNOME Wayland via Shell extension; X11 deferred |
| Extension role | Extension **draws the HUD** and handles all UI/focus/injection; daemon is headless core |
| Text editing | Editable field; rely on system OSK/AT (no custom keyboard) |
| Settings UI | Native extension `prefs.js` (Adwaita) |
| Tray | Native top-bar panel indicator in the extension; retire GTK3 `sasayaku-tray` on GNOME |
| Vulkan | In scope, as a separate workstream |
| Daemon lifecycle | XDG autostart + D-Bus activation (both already exist); manual start in dev |

## 5. Architecture

```
┌─────────────────────────────┐        D-Bus (session bus)        ┌──────────────────────────────┐
│  sasayaku-daemon (C++)      │  org.sasayaku.Daemon              │  sasayaku@… GNOME extension   │
│  headless core              │ <───────────────────────────────>│  (gjs / St, runs in Shell)    │
│                             │  methods + signals                │                               │
│  • PipeWire audio capture   │                                   │  • Panel indicator (top bar)  │
│  • whisper (Vulkan/CPU)     │  ── AudioLevel(d) ───────────────>│  • HUD overlay (St)           │
│  • AI/Ollama (OpenAI client)│  ── StateChanged(s) ─────────────>│  • Focus tracker (Meta)       │
│  • config / modes / vocab   │  ── TranscriptionComplete(s) ────>│  • Input injector (Clutter)   │
│  • D-Bus service            │  <── StartRecording/Stop/Toggle ──│  • Global hotkey              │
│                             │  <── Get/SetConfig, Models, … ────│  • D-Bus client               │
└─────────────────────────────┘                                   │  • prefs.js (Settings)        │
        ▲ autostart / D-Bus activation                            └──────────────────────────────┘
        │
   sasayaku-cli (scripting entry, calls D-Bus)
```

Mapping to macOS: the **daemon** plays the role of the shared macOS core (audio/whisper/AI);
the **extension** plays the role of `OverlayPanel` + `AppDelegate` (UI, overlay, hotkey,
menu-bar item, focus handling).

### 5.1 Daemon (C++) responsibilities

- Reuse existing core libs unchanged where possible: `core/` (audio_capture, audio_processor,
  whisper_wrapper, transcription), `ai/` (openai_client, prompt_templates), `utils/`
  (config_manager, clipboard). `window_tracker` and `auto_paste` are **not needed** on the
  GNOME path (the extension does focus + injection) — kept only if still referenced.
- **Remove the GTK4 HUD from the GNOME path.** `recording_window` and `settings_window`
  (GTK4) are no longer launched. The daemon becomes headless; it no longer links GTK4/libadwaita
  for the GNOME build. (The GTK4 UI may remain in-tree as legacy but is not built/used here.)
- Extend the `org.sasayaku.Daemon` D-Bus service (see §6).
- Initialize whisper with the Vulkan backend when `use_gpu` is set and a device is available;
  fall back to CPU otherwise (see §8).
- Emit `AudioLevel(d)` during recording and `StateChanged(s)` on state transitions.

### 5.2 Extension (`sasayaku@wmeddie.github`) structure

```
sasayaku@wmeddie.github/
  metadata.json        # uuid: sasayaku@wmeddie.github, shell-version: ["50"], settings-schema
  extension.js         # enable()/disable(); wires the pieces below
  hud.js               # St overlay: states, waveform, editable field, buttons
  indicator.js         # PanelMenu.Button: state icon + menu (Show HUD, mode, Settings, Quit)
  daemonClient.js      # Gio.DBusProxy wrapper for org.sasayaku.Daemon
  focusTracker.js      # remembers last non-HUD app window via global.display
  injector.js          # Clutter virtual keyboard (Ctrl+V, Enter, Tab) + St.Clipboard
  prefs.js             # Adwaita settings UI (talks to daemon via D-Bus)
  stylesheet.css       # HUD styling (dark, rounded, matches macOS OverlayView)
  schemas/             # org.gnome.shell.extensions.sasayaku.gschema.xml (hotkey, prefs cache)
```

- **HUD (`hud.js`):** a `St.Widget` added to `Main.layoutManager.uiGroup` (or a `St.BoxLayout`
  positioned bottom-center, like macOS). Because it lives in the Shell, it is drawn above app
  windows and does not take *window* focus. States: `idle`, `recording` (animated waveform from
  `AudioLevel`), `processing` (spinner), `done` (editable `St.Entry`/text + button row).
- **Focus tracker:** subscribe to `global.display.connect('notify::focus-window')`; whenever
  focus changes to a window that is **not** our HUD/Shell chrome, store it as `lastTargetWindow`.
  Snapshot this when recording starts so paste targets the right window.
- **Injector:** `Clutter.get_default_backend().get_default_seat().create_virtual_device(
  Clutter.InputDeviceType.KEYBOARD_DEVICE)`; use `notify_keyval(time, keyval, state)` to send
  Ctrl+V and Return. Clipboard via `St.Clipboard.get_default().set_text(CLIPBOARD, text)`.
- **Paste flow:** on "Paste", set clipboard → `lastTargetWindow.activate(global.get_current_time())`
  → inject Ctrl+V; on "Enter", inject Return. "Copy" only sets the clipboard. "Select All"/"Cut"
  operate on the HUD's editable field.
- **Indicator (`indicator.js`):** `PanelMenu.Button` with a microphone icon that changes on
  recording; menu items: Show HUD, mode submenu (from `GetModes`), Settings (opens prefs),
  Quit (calls daemon `Quit`).
- **Hotkey:** `Main.wm.addKeybinding('toggle-recording', settings, …, () => client.ToggleRecording())`.
- **prefs.js:** Adwaita window that reads/writes daemon config over D-Bus and drives model
  download with progress (see §6).

## 6. D-Bus Contract (`org.sasayaku.Daemon`, path `/org/sasayaku/Daemon`)

Existing (keep): `StartRecording`, `StopRecording`, `ToggleRecording`, `Quit`,
`GetStatus() → s`, signals `RecordingStarted`, `RecordingStopped`, `TranscriptionComplete(s text)`.
`ShowWindow`/`ShowSettings` become **no-ops or are removed** on the GNOME path (UI is the extension's).

New methods:

| Method | Signature | Purpose |
|--------|-----------|---------|
| `GetModes` | `() → a(ss)` | list of (id, name) for the menu/HUD |
| `GetCurrentMode` | `() → s` | current mode id |
| `SetMode` | `(s id)` | switch mode |
| `GetConfig` | `() → a{sv}` | settings for prefs (api url/key/model, whisper model, device, use_gpu) |
| `SetConfig` | `(a{sv})` | apply settings |
| `ListInputDevices` | `() → a(ss)` | (id, label) microphones |
| `ListWhisperModels` | `() → a(sssb)` | (filename, label, size, downloaded) |
| `DownloadModel` | `(s filename)` | start download (async; emits progress) |
| `ProcessText` | `(s text, s mode) → s` | optional: re-run AI on edited text |

New signals:

| Signal | Args | Purpose |
|--------|------|---------|
| `StateChanged` | `(s state)` | `idle`/`recording`/`processing`/`done` — drives HUD |
| `AudioLevel` | `(d level)` | 0..1 RMS for waveform during recording |
| `ModelDownloadProgress` | `(s filename, d fraction)` | prefs progress bar |
| `Error` | `(s message)` | surfaced in HUD/indicator |

`TranscriptionComplete(s)` continues to carry the final (post-AI) text. Consider adding a
`(s raw, s processed, s mode)` variant if the HUD needs both; decided during implementation.

## 7. Data Flow (sequence)

1. Idle: daemon running (autostart/activation); extension indicator shows idle mic.
2. User triggers (indicator click or hotkey) → extension snapshots `lastTargetWindow` →
   calls `ToggleRecording`.
3. Daemon: PipeWire capture starts → emits `StateChanged("recording")` + `AudioLevel(d)` stream.
4. Extension: HUD appears (bottom-center), animates waveform from `AudioLevel`.
5. User triggers again → `ToggleRecording` → daemon stops capture →
   `StateChanged("processing")` → whisper (Vulkan) → AI mode → `TranscriptionComplete(text)` +
   `StateChanged("done")`.
6. Extension: HUD shows editable result + buttons.
7. User edits (system OSK if needed), clicks **Copy** (clipboard) or **Paste** (activate target
   → clipboard → Ctrl+V) or **Enter** (Return), or **Re-record**.
8. HUD dismisses (button or timeout); focus naturally returns to the target.

## 8. Vulkan Workstream

- Rebuild whisper.cpp with `-DGGML_VULKAN=ON -DBUILD_SHARED_LIBS=ON` (shaders compiled via
  `glslc`). Produces `libggml-vulkan.so` alongside the existing libs.
- Update the Meson `find_library` set (currently whisper/ggml/ggml-base/ggml-cpu) to also pick
  up `ggml-vulkan` and keep the rpath entries.
- Runtime: whisper context params `use_gpu = config.use_gpu`. With the Vulkan backend compiled
  in, whisper/ggml auto-selects the Vulkan device. Verify device discovery; if none, log and run
  CPU.
- Verification: `whisper-bench`/`whisper-cli` from the build against a sample to confirm the
  Vulkan device is used and faster than CPU.
- Risk: shader compile cost at first run; AMD driver/Mesa RADV maturity (RX 9060 XT is new). CPU
  fallback keeps the app usable.

## 9. Build & Packaging

- **Daemon/CLI:** `meson` builds `sasayaku-daemon` (headless; no GTK4 link on GNOME path) and
  `sasayaku-cli`. The GTK4 UI libraries are dropped from the daemon's deps; the GTK3 tray target
  is not built.
- **whisper.cpp:** rebuilt with Vulkan (see §8).
- **Extension:** installed to `~/.local/share/gnome-shell/extensions/sasayaku@wmeddie.github/`. A
  `make install`-style step or script copies the `sasayaku@…/` tree and compiles gschemas
  (`glib-compile-schemas`). Enable with `gnome-extensions enable`. On Wayland, loading a new
  extension requires a logout/login (no live Shell restart).
- **Daemon lifecycle:** existing `org.sasayaku.Daemon.service` (D-Bus activation) and XDG
  autostart `.desktop` cover installed use. In dev, run `./build/src/sasayaku-daemon` manually;
  the extension detects absence and shows a hint.

## 10. Error Handling

- **Daemon not running:** extension `daemonClient` catches D-Bus errors; indicator shows a
  warning state and the HUD/menu offers guidance (or triggers D-Bus activation).
- **No Vulkan device:** daemon logs and falls back to CPU; surfaced via `Error` signal once.
- **Injection/activation fails** (e.g., target window closed): extension keeps text on clipboard
  and shows "Copied — paste manually" in the HUD.
- **Model missing:** daemon reports via `Error`; prefs prompts download.
- **GNOME version mismatch:** `metadata.json` pins `shell-version`; document the supported
  version.

## 11. Testing Strategy

- **Daemon:** unit/integration tests for config (Get/SetConfig round-trip), modes, and the
  D-Bus method handlers. A test harness can drive D-Bus with `gdbus call` and assert signals
  with `dbus-monitor`. Transcription tested against a fixed WAV → expected text (whisper is
  deterministic enough for a smoke assertion).
- **Vulkan:** benchmark whisper on a sample; assert Vulkan device selected (log/marker) and a
  speedup vs CPU.
- **Extension:** developed against a **nested** shell for safety —
  `dbus-run-session -- gnome-shell --nested --wayland` — using Looking Glass and `journalctl
  /usr/bin/gnome-shell` for logs. Manual test matrix: indicator states, HUD state transitions,
  waveform, editable field + system OSK, each button (Copy/Paste/Enter/Select All/Cut/Re-record),
  focus return to a target app (e.g. gedit, a terminal, a browser field).
- **End-to-end:** speak → transcribe → edit → paste into a target app on the real session.

## 12. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| GNOME API drift across versions | Pin `shell-version`; isolate Shell API use in small modules (`focusTracker`, `injector`) |
| Clutter virtual-device API changes (GNOME 50) | Verify early with a spike; fall back to clipboard-only paste if injection unavailable |
| PaperWM (user runs it) alters window activation | Test activation/focus under PaperWM specifically |
| New AMD GPU + RADV maturity | CPU fallback; verify with bench before relying on Vulkan |
| Wayland extension reload needs logout | Use nested shell for dev iteration |
| Clipboard ownership on Wayland | Use `St.Clipboard` (Shell-owned) for set; daemon clipboard via GTK `GdkClipboard` if needed |

## 13. Suggested Implementation Phases (for planning)

1. **Daemon D-Bus expansion + headless refactor** — extend interface (§6), emit
   `StateChanged`/`AudioLevel`, drop GTK4 HUD from the GNOME build. Verifiable via `gdbus`.
2. **Vulkan workstream** — rebuild whisper.cpp with Vulkan, wire `use_gpu`, verify speedup.
   (Independent; can run in parallel.)
3. **Extension skeleton + indicator + daemon client** — panel indicator, connect to daemon,
   toggle recording, reflect state.
4. **HUD overlay** — states, waveform from `AudioLevel`, editable result, button row.
5. **Focus tracking + injection** — snapshot target, activate, Ctrl+V/Enter; clipboard.
6. **Extension prefs** — Adwaita settings over D-Bus, including model download progress.
7. **Packaging + end-to-end verification** — install scripts, lifecycle, full manual matrix.

Each phase gets its own implementation plan via the writing-plans workflow.
