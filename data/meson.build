# D-Bus service file
dbus_service_conf = configuration_data()
dbus_service_conf.set('bindir', get_option('prefix') / get_option('bindir'))

configure_file(
  input: 'org.sasayaku.Daemon.service.in',
  output: 'org.sasayaku.Daemon.service',
  configuration: dbus_service_conf,
  install: true,
  install_dir: get_option('datadir') / 'dbus-1' / 'services',
)

# Desktop file (autostart)
desktop_conf = configuration_data()
desktop_conf.set('bindir', get_option('prefix') / get_option('bindir'))

configure_file(
  input: 'sasayaku-daemon.desktop.in',
  output: 'sasayaku-daemon.desktop',
  configuration: desktop_conf,
  install: true,
  install_dir: get_option('sysconfdir') / 'xdg' / 'autostart',
)

# Install default modes configuration
install_data('default_modes.json',
  install_dir: get_option('datadir') / 'sasayaku',
)

# Install app icon (if we create one later)
# install_data('sasayaku.svg',
#   install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
# )
