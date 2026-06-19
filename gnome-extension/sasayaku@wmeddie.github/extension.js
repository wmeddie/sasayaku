import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

import {DaemonClient} from './daemonClient.js';
import {FocusTracker} from './focusTracker.js';
import {Injector} from './injector.js';
import {SasayakuHud} from './hud.js';
import {SasayakuIndicator} from './indicator.js';

export default class SasayakuExtension extends Extension {
    enable() {
        this._settings = this.getSettings();

        this._client = new DaemonClient();
        this._client.connectDaemon({
            onState: (s) => this._onState(s),
            onAudioLevel: (l) => this._hud?.setLevel(l),
            onTranscription: (t) => {
                this._injector?.setClipboard(t);
                this._hud?.showResult(t);
            },
            onError: (m) => this._hud?.showError(m),
        });

        this._focus = new FocusTracker();
        this._focus.enable();

        this._injector = new Injector();

        this._hud = new SasayakuHud({
            onCopy: (text) => this._injector.setClipboard(text),
            onPaste: (text) => {
                const target = this._focus.target;
                this._hud.hide();
                this._injector.pasteText(target, text);
            },
            onEnter: () => {
                const target = this._focus.target;
                this._hud.hide();
                this._injector.sendEnter(target);
            },
            onRerecord: () => {
                this._hud.hide();
                this._focus.snapshot();
                this._client.startRecording();
            },
            onClose: () => this._hud.hide(),
        });

        this._indicator = new SasayakuIndicator(this._client, {
            onToggle: () => this._onToggle(),
            onOpenPrefs: () => this.openPreferences(),
        });
        Main.panel.addToStatusArea(this.uuid, this._indicator);

        Main.wm.addKeybinding(
            'toggle-recording',
            this._settings,
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW,
            () => this._onToggle());

        if (this._client.available) {
            this._indicator.setModes(this._client.getModes());
            this._indicator.setState(this._client.getStatus());
        }
        console.log(`[sasayaku] enabled; daemon available=${this._client.available}`);
    }

    _onToggle() {
        console.log(`[sasayaku] _onToggle; daemon available=${this._client.available}`);
        if (!this._client.available) {
            Main.notify('Sasayaku', 'The Sasayaku daemon is not running.');
            return;
        }
        // Snapshot the target window before toggling (harmless when stopping).
        this._focus.snapshot();
        this._client.toggleRecording();
    }

    _onState(state) {
        console.log(`[sasayaku] _onState: ${state}`);
        this._indicator?.setState(state);
        switch (state) {
        case 'recording':
            this._hud?.showRecording();
            break;
        case 'processing':
            this._hud?.showProcessing();
            break;
        // 'done' is paired with TranscriptionComplete -> showResult().
        // 'idle' leaves the HUD as-is (dismissed via Close/Paste).
        }
    }

    disable() {
        Main.wm.removeKeybinding('toggle-recording');
        this._indicator?.destroy();
        this._indicator = null;
        this._hud?.destroy();
        this._hud = null;
        this._injector?.destroy();
        this._injector = null;
        this._focus?.disable();
        this._focus = null;
        this._client?.destroy();
        this._client = null;
        this._settings = null;
    }
}
