import Adw from 'gi://Adw';

import {ExtensionPreferences} from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';

export default class SasayakuPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        const settings = this.getSettings();

        const page = new Adw.PreferencesPage();
        window.add(page);

        const shortcutGroup = new Adw.PreferencesGroup({
            title: 'Shortcut',
            description: 'Global hotkey to start/stop dictation, in accelerator syntax (e.g. <Control><Alt>space). Leave empty to disable.',
        });
        page.add(shortcutGroup);

        const row = new Adw.EntryRow({ title: 'Toggle recording' });
        const current = settings.get_strv('toggle-recording');
        row.text = current.length ? current[0] : '';
        row.connect('changed', () => {
            const value = row.text.trim();
            settings.set_strv('toggle-recording', value ? [value] : []);
        });
        shortcutGroup.add(row);

        const infoGroup = new Adw.PreferencesGroup({
            title: 'Daemon settings',
            description: 'API endpoint, model, microphone and whisper model are configured in ~/.config/sasayaku/config.json and applied by the daemon. A native settings UI is planned.',
        });
        page.add(infoGroup);
    }
}
