# Core library sources
core_sources = files(
  'core/whisper_wrapper.cpp',
  'core/audio_capture.cpp',
  'core/audio_processor.cpp',
  'core/transcription.cpp',
)

# AI module sources
ai_sources = files(
  'ai/openai_client.cpp',
  'ai/prompt_templates.cpp',
)

# Utility sources
utils_sources = files(
  'utils/clipboard.cpp',
  'utils/auto_paste.cpp',
  'utils/config_manager.cpp',
)

# Build core library
libsasayaku_core = static_library('sasayaku_core',
  core_sources,
  dependencies: [whisper_lib, pipewire_dep, spa_dep, threads_dep],
  include_directories: [whisper_include],
  install: false,
)

# Build AI library
libsasayaku_ai = static_library('sasayaku_ai',
  ai_sources,
  dependencies: [curl_dep, json_dep],
  install: false,
)

# Build utils library
libsasayaku_utils = static_library('sasayaku_utils',
  utils_sources,
  dependencies: [gio_dep, json_dep, gtk_dep],
  install: false,
)

# UI sources
ui_sources = files(
  'ui/recording_window.cpp',
  'ui/settings_window.cpp',
)

# UI library (used by daemon)
libsasayaku_ui = static_library('sasayaku_ui',
  ui_sources,
  dependencies: [gtk_dep, libadwaita_dep, gio_dep, curl_dep],
  install: false,
)

# Daemon sources
daemon_sources = files(
  'daemon/main.cpp',
  'daemon/dbus_service.cpp',
  'daemon/mode_manager.cpp',
  'daemon/window_tracker.cpp',
  'daemon/recording_coordinator.cpp',
)

# Daemon executable
sasayaku_daemon = executable('sasayaku-daemon',
  daemon_sources,
  link_with: [libsasayaku_core, libsasayaku_ai, libsasayaku_utils, libsasayaku_ui],
  dependencies: [gtk_dep, libadwaita_dep, gio_dep, threads_dep, whisper_lib],
  include_directories: [whisper_include],
  install: true,
)

# CLI tool
cli_sources = files(
  'cli/main.cpp',
)

sasayaku_cli = executable('sasayaku-cli',
  cli_sources,
  dependencies: [gio_dep],
  install: true,
)

# Unit tests
test_dbus_helpers = executable('test-dbus-helpers',
  ['daemon/dbus_helpers.cpp', 'daemon/dbus_helpers_test.cpp'],
  install: false,
)
test('dbus_helpers', test_dbus_helpers)
