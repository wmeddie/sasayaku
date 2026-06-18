// Tracks the most recently focused application window, so the HUD can hand
// focus back to it when pasting / pressing Enter. Runs inside GNOME Shell,
// so it can see Meta window focus that ordinary Wayland clients cannot.

export class FocusTracker {
    constructor() {
        this._lastWindow = null;
        this._focusId = 0;
    }

    enable() {
        this._focusId = global.display.connect('notify::focus-window', () => {
            const win = global.display.focus_window;
            // Keep the last *real* window; ignore transient nulls (e.g. when
            // shell chrome such as our HUD takes key focus).
            if (win)
                this._lastWindow = win;
        });
        if (global.display.focus_window)
            this._lastWindow = global.display.focus_window;
    }

    // Capture the current target explicitly (call when recording starts,
    // before the HUD appears). Returns the captured window (may be null).
    snapshot() {
        if (global.display.focus_window)
            this._lastWindow = global.display.focus_window;
        return this._lastWindow;
    }

    get target() {
        return this._lastWindow;
    }

    disable() {
        if (this._focusId)
            global.display.disconnect(this._focusId);
        this._focusId = 0;
        this._lastWindow = null;
    }
}
