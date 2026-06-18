// Clipboard + synthetic input, using GNOME Shell's privileged Clutter/St APIs.
// This is what makes Wayland paste-into-the-previous-window possible.
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import St from 'gi://St';

export class Injector {
    constructor() {
        this._seat = Clutter.get_default_backend().get_default_seat();
        this._kbd = this._seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
        this._timeouts = new Set();
    }

    setClipboard(text) {
        St.Clipboard.get_default().set_text(St.ClipboardType.CLIPBOARD, text ?? '');
    }

    // Press (and release) `keyval` while holding the given modifier keyvals.
    // The virtual input device expects timestamps in microseconds.
    _tap(keyval, modifiers = []) {
        if (!this._kbd)
            return;
        const t = GLib.get_monotonic_time();
        for (const m of modifiers)
            this._kbd.notify_keyval(t, m, Clutter.KeyState.PRESSED);
        this._kbd.notify_keyval(t, keyval, Clutter.KeyState.PRESSED);
        this._kbd.notify_keyval(t, keyval, Clutter.KeyState.RELEASED);
        for (const m of [...modifiers].reverse())
            this._kbd.notify_keyval(t, m, Clutter.KeyState.RELEASED);
    }

    // Re-activate the target window, then run `fn` after a short delay so the
    // compositor has finished moving focus before we synthesize input.
    _afterActivate(targetWindow, fn) {
        if (targetWindow) {
            try {
                targetWindow.activate(global.get_current_time());
            } catch (e) {
                logError(e, 'sasayaku: failed to activate target window');
            }
        }
        const id = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 150, () => {
            this._timeouts.delete(id);
            fn();
            return GLib.SOURCE_REMOVE;
        });
        this._timeouts.add(id);
    }

    // Set the clipboard to `text`, focus the target window, and send Ctrl+V.
    pasteText(targetWindow, text) {
        this.setClipboard(text);
        this._afterActivate(targetWindow, () => this._tap(Clutter.KEY_v, [Clutter.KEY_Control_L]));
    }

    // Focus the target window and send Return.
    sendEnter(targetWindow) {
        this._afterActivate(targetWindow, () => this._tap(Clutter.KEY_Return));
    }

    destroy() {
        for (const id of this._timeouts)
            GLib.source_remove(id);
        this._timeouts.clear();
        this._kbd = null;
        this._seat = null;
    }
}
