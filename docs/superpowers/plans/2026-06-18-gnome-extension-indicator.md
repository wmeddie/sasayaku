# GNOME Extension: Scaffold + Panel Indicator + Daemon Client Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A loadable GNOME Shell 50 extension (`sasayaku@wmeddie.github`) that shows a top-bar panel indicator, connects to the `org.sasayaku.Daemon` D-Bus service, toggles recording, lists/sets modes, and reflects recording state live.

**Architecture:** GJS ESM extension (GNOME 45+ module style). `daemonClient.js` wraps a `Gio.DBusProxy` for the daemon (methods + `StateChanged` signal). `indicator.js` is a `PanelMenu.Button` with a mic icon + menu (toggle, mode submenu). `extension.js` wires them together in `enable()`/`disable()`. Sources live in the repo under `gnome-extension/` and an `install.sh` copies them into `~/.local/share/gnome-shell/extensions/`.

**Tech Stack:** GJS (GNOME 50), GNOME Shell extension API (ESM), `Gio.DBusProxy`, `gnome-extensions` CLI.

## Global Constraints

- Extension UUID: `sasayaku@wmeddie.github`. Directory name must equal the UUID.
- `metadata.json` `shell-version`: `["50"]` (this machine is GNOME Shell 50.1).
- GNOME 45+ ESM modules only: `import ... from 'resource:///org/gnome/shell/...'`; `extension.js` default-exports a class extending `Extension`.
- D-Bus: name `org.sasayaku.Daemon`, path `/org/sasayaku/Daemon`, interface `org.sasayaku.Daemon` (from the daemon). Methods used: `ToggleRecording`, `GetStatus()→s`, `GetModes()→a(ss)`, `GetCurrentMode()→s`, `SetMode(s)`. Signal used: `StateChanged(s)`.
- Do **not** auto-start the daemon (use `Gio.DBusProxyFlags.DO_NOT_AUTO_START`); show "Daemon not running" when absent.
- `disable()` must remove the indicator and drop all references (GNOME review requirement).
- GPL-3.0 repo. Commit messages end with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

## Environment Note (verification limits)

A GNOME Shell extension cannot be runtime-tested in this dev tooling: on Wayland the shell only loads a new/changed extension after a **logout/login**, and the daemon needs the real session (mic/PipeWire/GPU). So each task's automated checks are **static** (`python3 -m json.tool`, `gnome-extensions pack`, install + `gnome-extensions info`). Behavioral checks are done by the user and are listed under "User live verification".

## Deferred (separate follow-up plans)

- HUD overlay (recording/processing/done states, waveform, editable result, buttons) — spec Phase 4.
- Focus tracking + Clutter input injection + clipboard (paste/Enter into the previous window) — spec Phase 5.
- Global hotkey (gschema + `Main.wm.addKeybinding`) and `prefs.js` settings — spec Phase 6.

---

## File Structure

- **Create:** `gnome-extension/sasayaku@wmeddie.github/metadata.json` — extension manifest.
- **Create:** `gnome-extension/sasayaku@wmeddie.github/daemonClient.js` — D-Bus proxy wrapper.
- **Create:** `gnome-extension/sasayaku@wmeddie.github/indicator.js` — panel button + menu.
- **Create:** `gnome-extension/sasayaku@wmeddie.github/extension.js` — enable/disable wiring.
- **Create:** `gnome-extension/sasayaku@wmeddie.github/stylesheet.css` — recording-state styling.
- **Create:** `gnome-extension/install.sh` — copy into the user extensions dir.

---

## Task 1: Extension scaffold that loads and shows a static indicator

**Files:**
- Create: `gnome-extension/sasayaku@wmeddie.github/metadata.json`
- Create: `gnome-extension/sasayaku@wmeddie.github/extension.js`
- Create: `gnome-extension/sasayaku@wmeddie.github/indicator.js`
- Create: `gnome-extension/sasayaku@wmeddie.github/stylesheet.css`
- Create: `gnome-extension/install.sh`

**Interfaces:**
- Consumes: nothing.
- Produces: `SasayakuIndicator` (GObject class, `PanelMenu.Button`) constructed as `new SasayakuIndicator(client)`; `setState(stateString)` and `setModes(arrayOfIdNamePairs)` methods used by Task 2. In Task 1 `client` is `null` and the indicator is icon-only.

- [ ] **Step 1: Create the manifest** — `gnome-extension/sasayaku@wmeddie.github/metadata.json`

```json
{
  "uuid": "sasayaku@wmeddie.github",
  "name": "Sasayaku",
  "description": "Panel indicator and (later) HUD for the Sasayaku voice dictation daemon.",
  "shell-version": ["50"],
  "url": "https://github.com/wmeddie/sasayaku",
  "version": 1
}
```

- [ ] **Step 2: Create the indicator** — `gnome-extension/sasayaku@wmeddie.github/indicator.js`

```javascript
import GObject from 'gi://GObject';
import St from 'gi://St';

import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';

export const SasayakuIndicator = GObject.registerClass(
class SasayakuIndicator extends PanelMenu.Button {
    _init(client) {
        super._init(0.0, 'Sasayaku');
        this._client = client;

        this._icon = new St.Icon({
            icon_name: 'audio-input-microphone-symbolic',
            style_class: 'system-status-icon',
        });
        this.add_child(this._icon);

        // Toggle recording
        this._toggleItem = new PopupMenu.PopupMenuItem('Start Recording');
        this._toggleItem.connect('activate', () => {
            if (this._client)
                this._client.toggleRecording();
        });
        this.menu.addMenuItem(this._toggleItem);

        // Mode submenu
        this._modesItem = new PopupMenu.PopupSubMenuMenuItem('Mode');
        this.menu.addMenuItem(this._modesItem);

        // Refresh when the menu opens
        this.menu.connect('open-state-changed', (_menu, open) => {
            if (open && this._client)
                this._refresh();
        });
    }

    setModes(modes) {
        this._modesItem.menu.removeAll();
        for (const [id, name] of modes) {
            const item = new PopupMenu.PopupMenuItem(name);
            item.connect('activate', () => {
                if (this._client)
                    this._client.setMode(id);
            });
            this._modesItem.menu.addMenuItem(item);
        }
    }

    setState(state) {
        const recording = state === 'recording';
        this._toggleItem.label.text = recording ? 'Stop Recording' : 'Start Recording';
        this._icon.icon_name = recording
            ? 'audio-input-microphone-high-symbolic'
            : 'audio-input-microphone-symbolic';
        if (recording)
            this.add_style_class_name('sasayaku-recording');
        else
            this.remove_style_class_name('sasayaku-recording');
    }

    _refresh() {
        if (!this._client.available) {
            this._toggleItem.label.text = 'Daemon not running';
            this._toggleItem.setSensitive(false);
            return;
        }
        this._toggleItem.setSensitive(true);
        this.setModes(this._client.getModes());
        this.setState(this._client.getStatus());
    }
});
```

- [ ] **Step 3: Create the extension entry point** — `gnome-extension/sasayaku@wmeddie.github/extension.js`

```javascript
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

import {SasayakuIndicator} from './indicator.js';

export default class SasayakuExtension extends Extension {
    enable() {
        // Task 2 replaces `null` with a DaemonClient instance.
        this._indicator = new SasayakuIndicator(null);
        Main.panel.addToStatusArea(this.uuid, this._indicator);
    }

    disable() {
        this._indicator?.destroy();
        this._indicator = null;
    }
}
```

- [ ] **Step 4: Create the stylesheet** — `gnome-extension/sasayaku@wmeddie.github/stylesheet.css`

```css
.sasayaku-recording {
    color: #e01b24;
}
```

- [ ] **Step 5: Create the install script** — `gnome-extension/install.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

UUID="sasayaku@wmeddie.github"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)/$UUID"
DEST_DIR="$HOME/.local/share/gnome-shell/extensions/$UUID"

rm -rf "$DEST_DIR"
mkdir -p "$(dirname "$DEST_DIR")"
cp -r "$SRC_DIR" "$DEST_DIR"

echo "Installed $UUID -> $DEST_DIR"
echo "On Wayland, log out and back in, then run:"
echo "  gnome-extensions enable $UUID"
```

- [ ] **Step 6: Static verification — manifest parses and the bundle validates**

Run:
```bash
chmod +x gnome-extension/install.sh
python3 -m json.tool gnome-extension/sasayaku@wmeddie.github/metadata.json >/dev/null && echo "metadata.json OK"
( cd gnome-extension/sasayaku@wmeddie.github && gnome-extensions pack --force --out-dir /tmp >/dev/null && echo "pack OK" )
```
Expected: `metadata.json OK` and `pack OK`. (`gnome-extensions pack` validates that `metadata.json` + `extension.js` are present and well-formed.)

- [ ] **Step 7: Install and confirm the shell sees it**

Run:
```bash
./gnome-extension/install.sh
gnome-extensions info sasayaku@wmeddie.github | head -5
```
Expected: install message, then `info` prints the extension (Name: Sasayaku; State likely `INITIALIZED` or `INACTIVE` until enabled).

- [ ] **Step 8: Commit**

```bash
git add gnome-extension/
git commit -m "$(cat <<'EOF'
Add GNOME extension scaffold with panel indicator

Loadable sasayaku@wmeddie.github extension (GNOME 50, ESM): a top-bar
mic indicator with a menu (toggle + mode submenu) and an install script.
Daemon wiring follows.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

**User live verification (after this task):** log out/in, `gnome-extensions enable sasayaku@wmeddie.github`, confirm a microphone icon appears in the top bar and its menu shows "Start Recording" + "Mode". Check `journalctl --user -b 0 -o cat /usr/bin/gnome-shell | grep -i sasayaku` for errors (should be none).

---

## Task 2: Daemon D-Bus client + wire toggle/modes/state

**Files:**
- Create: `gnome-extension/sasayaku@wmeddie.github/daemonClient.js`
- Modify: `gnome-extension/sasayaku@wmeddie.github/extension.js`

**Interfaces:**
- Consumes: `SasayakuIndicator(client)` with `setState(string)` / `setModes(pairs)` from Task 1.
- Produces: `DaemonClient` with `connectDaemon(onState)`, getter `available` (bool), `toggleRecording()`, `getStatus()→string`, `getModes()→[[id,name],…]`, `getCurrentMode()→string`, `setMode(id)`, `destroy()`.

- [ ] **Step 1: Create the D-Bus client** — `gnome-extension/sasayaku@wmeddie.github/daemonClient.js`

```javascript
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

const BUS_NAME = 'org.sasayaku.Daemon';
const OBJECT_PATH = '/org/sasayaku/Daemon';
const IFACE = 'org.sasayaku.Daemon';

export class DaemonClient {
    constructor() {
        this._proxy = null;
        this._stateChangedId = 0;
        this._onState = null;
    }

    connectDaemon(onState) {
        this._onState = onState;
        try {
            this._proxy = Gio.DBusProxy.new_for_bus_sync(
                Gio.BusType.SESSION,
                Gio.DBusProxyFlags.DO_NOT_AUTO_START,
                null, BUS_NAME, OBJECT_PATH, IFACE, null);
            this._stateChangedId = this._proxy.connectSignal(
                'StateChanged', (_proxy, _sender, [state]) => {
                    if (this._onState)
                        this._onState(state);
                });
        } catch (e) {
            logError(e, 'sasayaku: failed to connect to daemon');
            this._proxy = null;
        }
    }

    get available() {
        return this._proxy !== null && this._proxy.get_name_owner() !== null;
    }

    _call(method, params = null) {
        if (!this._proxy)
            return null;
        try {
            return this._proxy.call_sync(method, params, Gio.DBusCallFlags.NONE, -1, null);
        } catch (e) {
            logError(e, `sasayaku: ${method} failed`);
            return null;
        }
    }

    toggleRecording() {
        this._call('ToggleRecording');
    }

    getStatus() {
        const r = this._call('GetStatus');
        return r ? r.deepUnpack()[0] : 'unknown';
    }

    getModes() {
        const r = this._call('GetModes');
        return r ? r.deepUnpack()[0] : [];
    }

    getCurrentMode() {
        const r = this._call('GetCurrentMode');
        return r ? r.deepUnpack()[0] : '';
    }

    setMode(id) {
        this._call('SetMode', new GLib.Variant('(s)', [id]));
    }

    destroy() {
        if (this._proxy && this._stateChangedId)
            this._proxy.disconnectSignal(this._stateChangedId);
        this._stateChangedId = 0;
        this._proxy = null;
        this._onState = null;
    }
}
```

- [ ] **Step 2: Wire the client into `extension.js`**

Overwrite `gnome-extension/sasayaku@wmeddie.github/extension.js` with:
```javascript
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

import {DaemonClient} from './daemonClient.js';
import {SasayakuIndicator} from './indicator.js';

export default class SasayakuExtension extends Extension {
    enable() {
        this._client = new DaemonClient();
        this._client.connectDaemon((state) => {
            this._indicator?.setState(state);
        });

        this._indicator = new SasayakuIndicator(this._client);
        Main.panel.addToStatusArea(this.uuid, this._indicator);

        // Initial paint if the daemon is already up.
        if (this._client.available) {
            this._indicator.setModes(this._client.getModes());
            this._indicator.setState(this._client.getStatus());
        }
    }

    disable() {
        this._indicator?.destroy();
        this._indicator = null;
        this._client?.destroy();
        this._client = null;
    }
}
```

- [ ] **Step 3: Static verification — bundle still validates and reinstalls**

Run:
```bash
( cd gnome-extension/sasayaku@wmeddie.github && gnome-extensions pack --force --out-dir /tmp >/dev/null && echo "pack OK" )
./gnome-extension/install.sh
gnome-extensions info sasayaku@wmeddie.github | head -3
```
Expected: `pack OK`, install message, and `info` prints the extension.

- [ ] **Step 4: Commit**

```bash
git add gnome-extension/
git commit -m "$(cat <<'EOF'
Wire GNOME indicator to the daemon over D-Bus

DaemonClient wraps a Gio.DBusProxy for org.sasayaku.Daemon: toggle
recording, list/set modes, read status, and follow the StateChanged
signal. The indicator now reflects live recording state.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

**User live verification (after this task):**
1. Start the daemon in a terminal: `./build/src/sasayaku-daemon`.
2. Log out/in (Wayland) if not already done, then `gnome-extensions enable sasayaku@wmeddie.github`.
3. Click the top-bar mic → "Start Recording"; speak; the icon turns red and the label becomes "Stop Recording" (driven by `StateChanged`). Click again to stop; transcription completes in the daemon log.
4. Open the menu → "Mode" submenu lists Email/Note/Voice to Text; selecting one calls `SetMode` (confirm in the daemon: `gdbus call ... GetCurrentMode`).
5. Quit the daemon → reopen the menu → "Daemon not running" shown, toggle disabled.
6. `journalctl --user -b 0 -o cat /usr/bin/gnome-shell | grep -i sasayaku` shows no errors.

---

## Self-Review

**1. Spec coverage (design §5.2 "Extension structure"):** `metadata.json`, `extension.js`, `daemonClient.js`, `indicator.js`, `stylesheet.css` created (Tasks 1–2). Panel indicator with state icon + menu (Task 1/2). Daemon D-Bus client consuming `GetModes`/`GetCurrentMode`/`SetMode`/`GetStatus`/`ToggleRecording` + `StateChanged` (Task 2) — matches the Phase-1 daemon contract. HUD (`hud.js`), `focusTracker.js`, `injector.js`, `prefs.js`, `schemas/`, global hotkey are explicitly deferred (Deferred section) to later phase plans.

**2. Placeholder scan:** No TBD/TODO; every file has complete content; verification steps have exact commands + expected output. Task 1's `client === null` path is intentional (scaffold), replaced wholesale in Task 2 — not a placeholder.

**3. Type consistency:** `DaemonClient` methods defined in Task 2 (`connectDaemon`, `available`, `toggleRecording`, `getStatus`, `getModes`, `getCurrentMode`, `setMode`, `destroy`) match the calls made by `indicator.js` (`this._client.toggleRecording/setMode/getModes/getStatus/available`, Task 1) and `extension.js` (Task 2). `setState(string)`/`setModes(pairs)` on the indicator (Task 1) match the calls in `extension.js` and the `StateChanged` callback (Task 2). D-Bus method/signal names match the daemon's introspection from the prior plan.
