// D-Bus client for the headless Sasayaku daemon (org.sasayaku.Daemon).
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

const BUS_NAME = 'org.sasayaku.Daemon';
const OBJECT_PATH = '/org/sasayaku/Daemon';
const IFACE = 'org.sasayaku.Daemon';

export class DaemonClient {
    constructor() {
        this._proxy = null;
        this._signalIds = [];
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
            return;
        }

        const connect = (name, fn) => {
            this._signalIds.push(this._proxy.connectSignal(name, fn));
        };
        connect('StateChanged', (_p, _s, [state]) => this._handlers.onState?.(state));
        connect('AudioLevel', (_p, _s, [level]) => this._handlers.onAudioLevel?.(level));
        connect('TranscriptionComplete', (_p, _s, [text]) => this._handlers.onTranscription?.(text));
        connect('Error', (_p, _s, [msg]) => this._handlers.onError?.(msg));
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

    toggleRecording() { this._call('ToggleRecording'); }
    startRecording() { this._call('StartRecording'); }
    stopRecording() { this._call('StopRecording'); }

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
        if (this._proxy) {
            for (const id of this._signalIds)
                this._proxy.disconnectSignal(id);
        }
        this._signalIds = [];
        this._proxy = null;
        this._handlers = {};
    }
}
