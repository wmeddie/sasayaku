// Top-bar panel indicator: recording-state icon + menu (toggle, modes, settings).
import GObject from 'gi://GObject';
import St from 'gi://St';

import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';

export const SasayakuIndicator = GObject.registerClass(
class SasayakuIndicator extends PanelMenu.Button {
    // callbacks: { onToggle(), onOpenPrefs() }
    _init(client, callbacks = {}) {
        super._init(0.0, 'Sasayaku');
        this._client = client;
        this._cb = callbacks;

        this._icon = new St.Icon({
            icon_name: 'audio-input-microphone-symbolic',
            style_class: 'system-status-icon',
        });
        this.add_child(this._icon);

        this._toggleItem = new PopupMenu.PopupMenuItem('Start Recording');
        this._toggleItem.connect('activate', () => this._cb.onToggle?.());
        this.menu.addMenuItem(this._toggleItem);

        this._modesItem = new PopupMenu.PopupSubMenuMenuItem('Mode');
        this.menu.addMenuItem(this._modesItem);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        const settingsItem = new PopupMenu.PopupMenuItem('Settings');
        settingsItem.connect('activate', () => this._cb.onOpenPrefs?.());
        this.menu.addMenuItem(settingsItem);

        this.menu.connect('open-state-changed', (_menu, open) => {
            if (open)
                this._refresh();
        });
    }

    setModes(modes) {
        this._modesItem.menu.removeAll();
        for (const [id, name] of modes) {
            const item = new PopupMenu.PopupMenuItem(name);
            item.connect('activate', () => this._client.setMode(id));
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
