// The floating recording/processing/result overlay, drawn inside GNOME Shell
// (so it never becomes a focus-stealing application window).
import GObject from 'gi://GObject';
import St from 'gi://St';
import Pango from 'gi://Pango';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

const NUM_BARS = 28;
const BAR_MAX_PX = 48;

export const SasayakuHud = GObject.registerClass(
class SasayakuHud extends St.BoxLayout {
    // callbacks: { onCopy(text), onPaste(text), onEnter(), onRerecord(), onClose() }
    _init(callbacks) {
        super._init({
            style_class: 'sasayaku-hud',
            vertical: true,
            reactive: true,
            track_hover: true,
            can_focus: true,
            visible: false,
        });
        this._cb = callbacks ?? {};
        this._levels = [];

        this._statusLabel = new St.Label({
            style_class: 'sasayaku-status',
            text: '',
        });
        this.add_child(this._statusLabel);

        // Waveform
        this._waveform = new St.BoxLayout({ style_class: 'sasayaku-waveform' });
        this._bars = [];
        for (let i = 0; i < NUM_BARS; i++) {
            const bar = new St.Widget({ style_class: 'sasayaku-bar', width: 6, height: 2 });
            this._waveform.add_child(bar);
            this._bars.push(bar);
        }
        this.add_child(this._waveform);

        // Editable transcription
        this._entry = new St.Entry({
            style_class: 'sasayaku-entry',
            can_focus: true,
            visible: false,
        });
        const ct = this._entry.clutter_text;
        ct.set_single_line_mode(false);
        ct.set_activatable(false);
        ct.set_line_wrap(true);
        ct.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR);
        this.add_child(this._entry);

        // Buttons
        this._buttonBox = new St.BoxLayout({ style_class: 'sasayaku-buttons', visible: false });
        this._addButton('Select All', () => {
            const text = ct.get_text() ?? '';
            ct.set_selection(0, text.length);
            global.stage.set_key_focus(ct);
        });
        this._addButton('Copy', () => this._cb.onCopy?.(this.getText()));
        this._addButton('Cut', () => {
            this._cb.onCopy?.(this.getText());
            this.setText('');
        });
        this._addButton('Paste', () => this._cb.onPaste?.(this.getText()));
        this._addButton('Enter', () => this._cb.onEnter?.());
        this._addButton('Re-record', () => this._cb.onRerecord?.());
        this._addButton('Close', () => this._cb.onClose?.());
        this.add_child(this._buttonBox);

        // Fixed width (St CSS does not reliably honor `width`); children wrap within.
        this.set_width(600);

        Main.layoutManager.addChrome(this, { affectsInputRegion: true });

        // Reposition whenever our size changes.
        this.connect('notify::height', () => this._reposition());
        this.connect('notify::width', () => this._reposition());
    }

    _addButton(label, onClick) {
        const button = new St.Button({
            style_class: 'sasayaku-button',
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

    setLevel(level) {
        this._levels.push(level);
        if (this._levels.length > NUM_BARS)
            this._levels.shift();
        for (let i = 0; i < NUM_BARS; i++) {
            const v = this._levels[i] ?? 0;
            this._bars[i].set_height(Math.max(2, Math.round(Math.min(1, v) * BAR_MAX_PX)));
        }
    }

    showRecording() {
        this._levels = [];
        this._statusLabel.text = 'Recording… speak now';
        this._waveform.visible = true;
        this._entry.visible = false;
        this._buttonBox.visible = false;
        this._present();
    }

    showProcessing() {
        this._statusLabel.text = 'Transcribing…';
        this._waveform.visible = false;
        this._entry.visible = false;
        this._buttonBox.visible = false;
        this._present();
    }

    showResult(text) {
        this._statusLabel.text = 'Review, then Copy / Paste / Enter';
        this._waveform.visible = false;
        this.setText(text);
        this._entry.visible = true;
        this._buttonBox.visible = true;
        this._present();
        global.stage.set_key_focus(this._entry.clutter_text);
    }

    showError(message) {
        this._statusLabel.text = `⚠ ${message}`;
        this._waveform.visible = false;
        this._entry.visible = false;
        this._buttonBox.visible = false;
        this._present();
    }

    _present() {
        this.show();
        this._reposition();
    }

    hide() {
        if (global.stage.get_key_focus() === this._entry.clutter_text)
            global.stage.set_key_focus(null);
        super.hide();
    }

    destroy() {
        Main.layoutManager.removeChrome(this);
        super.destroy();
    }
});
