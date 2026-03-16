project('sasayaku', ['c', 'cpp'],
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'cpp_std=c++17',
    'c_std=c11',
    'warning_level=2',
    'buildtype=release',
  ]
)

# Dependencies
gtk_dep = dependency('gtk4', version: '>= 4.0')
libadwaita_dep = dependency('libadwaita-1', version: '>= 1.0')
gio_dep = dependency('gio-2.0', version: '>= 2.66')
pipewire_dep = dependency('libpipewire-0.3', version: '>= 0.3')
spa_dep = dependency('libspa-0.2', version: '>= 0.2')
threads_dep = dependency('threads')
curl_dep = dependency('libcurl', version: '>= 7.68')

# JSON library
json_dep = dependency('nlohmann_json', version: '>= 3.9', required: false)
if not json_dep.found()
  # Fall back to subproject or system include
  json_dep = declare_dependency(
    include_directories: include_directories('third_party/json/include')
  )
endif

# whisper.cpp as subproject (build separately)
whisper_cpp_dir = meson.current_source_dir() / 'whisper.cpp'
whisper_include = include_directories('whisper.cpp/include', 'whisper.cpp/ggml/include')

# We'll link against pre-built whisper.cpp libraries
whisper_lib = meson.get_compiler('cpp').find_library('whisper',
  dirs: [whisper_cpp_dir / 'build'],
  required: false
)

if not whisper_lib.found()
  # whisper.cpp not built yet, we'll handle this in a custom target
  message('whisper.cpp not found in whisper.cpp/build - will need to build it first')
  whisper_lib = declare_dependency()
endif

# Subdirectories
subdir('src')
subdir('data')

# Summary
summary({
  'prefix': get_option('prefix'),
  'bindir': get_option('bindir'),
  'datadir': get_option('datadir'),
}, section: 'Directories')

summary({
  'GTK4': gtk_dep.found(),
  'libadwaita': libadwaita_dep.found(),
  'PipeWire': pipewire_dep.found(),
  'libcurl': curl_dep.found(),
}, section: 'Dependencies')
