// The floating recording/processing/result overlay, drawn inside GNOME Shell
// (so it never becomes a focus-stealing application window). Styled to match
// the macOS dark-glass overlay: thin white waveform, colored status dot.
import GObject from 'gi://GObject';
import St from 'gi://St';
import Clutter from 'gi://Clutter';
import Pango from 'gi://Pango';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

const HUD_WIDTH = 580;
const NUM_BARS = 64;
const WAVE_HEIGHT = 64;

export const SasayakuHud = GObject.registerClass(
class SasayakuHud extends St.BoxLayout {
    // callbacks: { onCopy(text), onPaste(text), onEnter(), onRerecord(), onClose() }
    _init(callbacks) {
        super._init({
            style_class: 'sasayaku-hud',
            orientation: Clutter.Orientation.VERTICAL,
            reactive: true,
            can_focus: true,
            visible: false,
        });
        this._cb = callbacks ?? {};
        this._levels = [];

        // Status row: colored dot + label.
        const statusRow = new St.BoxLayout({ style_class: 'sasayaku-statusrow' });
        this._dot = new St.Widget({
            style_class: 'sasayaku-dot',
            width: 10,
            height: 10,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._statusLabel = new St.Label({ style_class: 'sasayaku-status', text: '' });
        statusRow.add_child(this._dot);
        statusRow.add_child(this._statusLabel);
        this.add_child(statusRow);

        // Waveform — thin white bars, centered, heights driven by AudioLevel.
        this._waveform = new St.BoxLayout({ style_class: 'sasayaku-waveform' });
        this._waveform.set_height(WAVE_HEIGHT);
        this._bars = [];
        for (let i = 0; i < NUM_BARS; i++) {
            const bar = new St.Widget({
                style_class: 'sasayaku-bar',
                width: 3,
                height: 2,
                y_expand: false,
                y_align: Clutter.ActorAlign.CENTER,
            });
            this._waveform.add_child(bar);
            this._bars.push(bar);
        }
        this.add_child(this._waveform);

        // Editable transcription.
        this._entry = new St.Entry({ style_class: 'sasayaku-entry', can_focus: true, visible: false });
        const ct = this._entry.clutter_text;
        ct.set_single_line_mode(false);
        ct.set_activatable(false);
        ct.set_line_wrap(true);
        ct.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR);
        ct.connect('key-press-event', (_a, event) => this._onKeyPress(event));
        this.add_child(this._entry);

        // Buttons. Paste is the one accented (primary) action.
        this._buttonBox = new St.BoxLayout({ style_class: 'sasayaku-buttons', visible: false });
        this._addButton('Select All', false, () => {
            const text = ct.get_text() ?? '';
            ct.set_selection(0, text.length);
            global.stage.set_key_focus(ct);
        });
        this._addButton('Copy', false, () => this._cb.onCopy?.(this.getText()));
        this._addButton('Cut', false, () => {
            this._cb.onCopy?.(this.getText());
            this.setText('');
        });
        this._addButton('Paste', true, () => this._cb.onPaste?.(this.getText()));
        this._addButton('Enter', false, () => this._cb.onEnter?.());
        this._addButton('Re-record', false, () => this._cb.onRerecord?.());
        this._addButton('Close', false, () => this._cb.onClose?.());
        this.add_child(this._buttonBox);

        this.set_width(HUD_WIDTH);
        Main.layoutManager.addChrome(this);

        this.connect('key-press-event', (_a, event) => this._onKeyPress(event));
        this.connect('notify::height', () => this._reposition());
        this.connect('notify::width', () => this._reposition());
        this._monitorsId = Main.layoutManager.connect('monitors-changed', () => this._reposition());
    }

    _onKeyPress(event) {
        if (event.get_key_symbol() === Clutter.KEY_Escape) {
            this._cb.onClose?.();
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _addButton(label, primary, onClick) {
        const button = new St.Button({
            style_class: primary ? 'sasayaku-button sasayaku-button-primary' : 'sasayaku-button',
            label,
            can_focus: true,
        });
        button.connect('clicked', () => onClick());
        this._buttonBox.add_child(button);
        return button;
    }

    _reposition() {
        const mon = Main.layoutManager.primaryMonitor;
        if (!mon)
            return;
        const x = Math.round(mon.x + (mon.width - this.width) / 2);
        const y = Math.round(mon.y + mon.height - this.height - 96);
        this.set_position(x, y);
    }

    getText() {
        return this._entry.clutter_text.get_text() ?? '';
    }

    setText(text) {
        this._entry.clutter_text.set_text(text ?? '');
    }

    _setStatus(stateClass, text) {
        this._dot.style_class = `sasayaku-dot sasayaku-dot-${stateClass}`;
        this._statusLabel.text = text;
    }

    setLevel(level) {
        // Log-scale like the macOS waveform so quiet speech is still visible.
        const v = level > 0 ? Math.log10(1 + level * 100) / Math.log10(101) : 0;
        this._levels.push(Math.max(0, Math.min(1, v)));
        if (this._levels.length > NUM_BARS)
            this._levels.shift();
        for (let i = 0; i < NUM_BARS; i++) {
            const lv = this._levels[i] ?? 0;
            this._bars[i].set_height(Math.max(2, Math.round(lv * WAVE_HEIGHT)));
        }
    }

    showRecording() {
        this._levels = [];
        for (const bar of this._bars)
            bar.set_height(2);
        this._waveform.opacity = 255;
        this._setStatus('recording', 'Recording — speak now');
        this._waveform.visible = true;
        this._entry.visible = false;
        this._buttonBox.visible = false;
        this._present();
        global.stage.set_key_focus(this);
    }

    showProcessing() {
        this._waveform.opacity = 90;
        this._setStatus('processing', 'Transcribing…');
        this._waveform.visible = true;
        this._entry.visible = false;
        this._buttonBox.visible = false;
        this._present();
        global.stage.set_key_focus(this);
    }

    showResult(text) {
        this._setStatus('done', 'Review, then paste — Esc to dismiss');
        this._waveform.visible = false;
        this.setText(text);
        this._entry.visible = true;
        this._buttonBox.visible = true;
        this._present();
        global.stage.set_key_focus(this._entry.clutter_text);
    }

    showError(message) {
        this._setStatus('error', message);
        this._waveform.visible = false;
        this._entry.visible = false;
        this._buttonBox.visible = false;
        this._present();
        global.stage.set_key_focus(this);
    }

    _present() {
        this.show();
        this._reposition();
    }

    _releaseFocus() {
        const focus = global.stage.get_key_focus();
        if (focus && (focus === this || this.contains(focus)))
            global.stage.set_key_focus(null);
    }

    hide() {
        this._releaseFocus();
        super.hide();
    }

    destroy() {
        this._releaseFocus();
        if (this._monitorsId)
            Main.layoutManager.disconnect(this._monitorsId);
        this._monitorsId = 0;
        Main.layoutManager.removeChrome(this);
        super.destroy();
    }
});
