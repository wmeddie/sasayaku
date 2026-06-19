// D-Bus client for the headless Sasayaku daemon (org.sasayaku.Daemon).
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

const BUS_NAME = 'org.sasayaku.Daemon';
const OBJECT_PATH = '/org/sasayaku/Daemon';
const IFACE = 'org.sasayaku.Daemon';

export class DaemonClient {
    constructor() {
        this._proxy = null;
        this._subId = 0;
        this._handlers = {};
    }

    // handlers: { onState(state), onAudioLevel(level), onTranscription(text), onError(msg) }
    connectDaemon(handlers = {}) {
        this._handlers = handlers;
        try {
            this._proxy = Gio.DBusProxy.new_for_bus_sync(
                Gio.BusType.SESSION,
                Gio.DBusProxyFlags.DO_NOT_AUTO_START,
                null, BUS_NAME, OBJECT_PATH, IFACE, null);
        } catch (e) {
            logError(e, 'sasayaku: failed to create daemon proxy');
            this._proxy = null;
        }

        if (this._proxy) {
            console.log(`[sasayaku] proxy created; nameOwner=${this._proxy.get_name_owner()}`);
            this._proxy.connect('notify::g-name-owner', () => {
                console.log(`[sasayaku] g-name-owner -> ${this._proxy.get_name_owner()}`);
            });
        }

        // Subscribe to the daemon's signals directly on the session bus with
        // sender=null. A GDBusProxy created while the name is unowned (the daemon
        // starts after login) misses signals from the late owner; a sender=null
        // connection subscription matches them regardless of start order/restarts.
        this._subId = Gio.DBus.session.signal_subscribe(
            null, IFACE, null, OBJECT_PATH, null, Gio.DBusSignalFlags.NONE,
            (_conn, _sender, _path, _iface, signalName, params) => {
                const args = params.deepUnpack();
                switch (signalName) {
                case 'StateChanged':
                    console.log(`[sasayaku] signal StateChanged: ${args[0]}`);
                    this._handlers.onState?.(args[0]);
                    break;
                case 'AudioLevel':
                    this._handlers.onAudioLevel?.(args[0]);
                    break;
                case 'TranscriptionComplete':
                    console.log(`[sasayaku] signal TranscriptionComplete: ${args[0].length} chars`);
                    this._handlers.onTranscription?.(args[0]);
                    break;
                case 'Error':
                    console.log(`[sasayaku] signal Error: ${args[0]}`);
                    this._handlers.onError?.(args[0]);
                    break;
                }
            });
        console.log('[sasayaku] subscribed to daemon signals (sender=null)');
    }

    get available() {
        return this._proxy !== null && this._proxy.get_name_owner() !== null;
    }

    // Ask the session bus to D-Bus-activate the daemon. Works once the
    // org.sasayaku.Daemon.service file is installed (see meson install). Async,
    // so login is never blocked while the daemon loads its model.
    ensureRunning() {
        if (this.available)
            return;
        Gio.DBus.session.call(
            'org.freedesktop.DBus', '/org/freedesktop/DBus', 'org.freedesktop.DBus',
            'StartServiceByName', new GLib.Variant('(su)', [BUS_NAME, 0]),
            null, Gio.DBusCallFlags.NONE, -1, null,
            (conn, res) => {
                try {
                    conn.call_finish(res);
                    console.log('[sasayaku] daemon activation requested');
                } catch (e) {
                    console.log(`[sasayaku] daemon activation failed (is it installed?): ${e.message}`);
                }
            });
    }

    _call(method, params = null) {
        if (!this._proxy)
            return null;
        try {
            // Bounded timeout: a stuck daemon must not freeze GNOME Shell.
            return this._proxy.call_sync(method, params, Gio.DBusCallFlags.NONE, 1000, null);
        } catch (e) {
            logError(e, `sasayaku: ${method} failed`);
            return null;
        }
    }

    // Fire-and-forget async call: never blocks the GNOME Shell main loop.
    // A synchronous call here can deadlock (the daemon may call back into the
    // Shell) and would stall delivery of StateChanged/AudioLevel signals.
    _callAsync(method, params = null) {
        if (!this._proxy)
            return;
        this._proxy.call(method, params, Gio.DBusCallFlags.NONE, -1, null, (proxy, res) => {
            try {
                proxy.call_finish(res);
                console.log(`[sasayaku] call ${method} -> ok`);
            } catch (e) {
                console.log(`[sasayaku] call ${method} -> ERROR: ${e.message}`);
            }
        });
    }

    toggleRecording() { this._callAsync('ToggleRecording'); }
    startRecording() { this._callAsync('StartRecording'); }
    stopRecording() { this._callAsync('StopRecording'); }

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
        this._callAsync('SetMode', new GLib.Variant('(s)', [id]));
    }

    destroy() {
        if (this._subId) {
            Gio.DBus.session.signal_unsubscribe(this._subId);
            this._subId = 0;
        }
        this._proxy = null;
        this._handlers = {};
    }
}
