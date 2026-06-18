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

# We'll link against pre-built whisper.cpp libraries.
# Current whisper.cpp emits shared libs under build/src and build/ggml/src.
cpp_compiler = meson.get_compiler('cpp')
whisper_build = whisper_cpp_dir / 'build'
whisper_libdir = whisper_build / 'src'
ggml_libdir = whisper_build / 'ggml' / 'src'

# The Vulkan GPU backend, when built (-DGGML_VULKAN=ON), lands in a ggml-vulkan subdir.
ggml_vulkan_libdir = ggml_libdir / 'ggml-vulkan'

whisper_found = cpp_compiler.find_library('whisper', dirs: [whisper_libdir], required: false)
ggml_found = cpp_compiler.find_library('ggml', dirs: [ggml_libdir], required: false)
ggml_base_found = cpp_compiler.find_library('ggml-base', dirs: [ggml_libdir], required: false)
ggml_cpu_found = cpp_compiler.find_library('ggml-cpu', dirs: [ggml_libdir], required: false)
# Optional GPU backend; present only when whisper.cpp was built with -DGGML_VULKAN=ON.
ggml_vulkan_found = cpp_compiler.find_library('ggml-vulkan', dirs: [ggml_vulkan_libdir], required: false)

whisper_deps = [whisper_found, ggml_found, ggml_base_found, ggml_cpu_found]
whisper_rpaths = ['-Wl,-rpath,' + whisper_libdir, '-Wl,-rpath,' + ggml_libdir]
if ggml_vulkan_found.found()
  whisper_deps += ggml_vulkan_found
  whisper_rpaths += '-Wl,-rpath,' + ggml_vulkan_libdir
endif

if whisper_found.found()
  # Bundle whisper + ggml and bake in an rpath so the daemon finds the .so's at runtime.
  whisper_lib = declare_dependency(
    dependencies: whisper_deps,
    include_directories: whisper_include,
    link_args: whisper_rpaths,
  )
else
  # whisper.cpp not built yet - build it first (see README)
  message('whisper.cpp not found in ' + whisper_libdir + ' - will need to build it first')
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
  'PipeWire': pipewire_dep.found(),
  'libcurl': curl_dep.found(),
}, section: 'Dependencies')
