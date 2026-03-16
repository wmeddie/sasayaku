cmake_minimum_required(VERSION 3.10)
project(sasayaku VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Find dependencies
find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)  # For tray icon only
pkg_check_modules(LIBADWAITA libadwaita-1)  # Optional
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(APPINDICATOR REQUIRED ayatana-appindicator3-0.1)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
pkg_check_modules(SPA REQUIRED libspa-0.2)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# whisper.cpp libraries
set(WHISPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp")
set(WHISPER_BUILD_DIR "${WHISPER_DIR}/build")

# Core library sources
set(CORE_SOURCES
    src/core/whisper_wrapper.cpp
    src/core/audio_capture.cpp
    src/core/audio_processor.cpp
    src/core/transcription.cpp
)

# AI sources
set(AI_SOURCES
    src/ai/openai_client.cpp
    src/ai/prompt_templates.cpp
)

# Utils sources
set(UTILS_SOURCES
    src/utils/config_manager.cpp
    src/utils/clipboard.cpp
    src/utils/auto_paste.cpp
)

# UI sources
set(UI_SOURCES
    src/ui/recording_window.cpp
    src/ui/settings_window.cpp
)

# Daemon sources
set(DAEMON_SOURCES
    src/daemon/main.cpp
    src/daemon/dbus_service.cpp
    src/daemon/mode_manager.cpp
    src/daemon/window_tracker.cpp
    src/daemon/recording_coordinator.cpp
    ${UI_SOURCES}
)

# Toggle binary source
set(TOGGLE_SOURCES
    src/cli/toggle.cpp
)

# Tray helper binary source (separate GTK3 process)
set(TRAY_HELPER_SOURCES
    src/tray/tray_helper.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${WHISPER_DIR}/include
    ${WHISPER_DIR}/ggml/include
    ${GTK4_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${APPINDICATOR_INCLUDE_DIRS}
    ${PIPEWIRE_INCLUDE_DIRS}
    ${SPA_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

if(LIBADWAITA_FOUND)
    include_directories(${LIBADWAITA_INCLUDE_DIRS})
endif()

# Link directories
link_directories(
    ${WHISPER_BUILD_DIR}/src
    ${WHISPER_BUILD_DIR}/ggml/src
    ${GTK4_LIBRARY_DIRS}
    ${GTK3_LIBRARY_DIRS}
    ${GIO_LIBRARY_DIRS}
    ${APPINDICATOR_LIBRARY_DIRS}
    ${PIPEWIRE_LIBRARY_DIRS}
    ${SPA_LIBRARY_DIRS}
)

if(LIBADWAITA_FOUND)
    link_directories(${LIBADWAITA_LIBRARY_DIRS})
endif()

# Core library
add_library(sasayaku_core STATIC ${CORE_SOURCES})
target_link_libraries(sasayaku_core
    whisper
    ggml
    ${PIPEWIRE_LIBRARIES}
    ${SPA_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# AI library
add_library(sasayaku_ai STATIC ${AI_SOURCES})
target_link_libraries(sasayaku_ai
    ${CURL_LIBRARIES}
)

# Utils library
add_library(sasayaku_utils STATIC ${UTILS_SOURCES})
target_link_libraries(sasayaku_utils
    ${GTK4_LIBRARIES}
    ${GIO_LIBRARIES}
)


# Daemon executable
add_executable(sasayaku-daemon ${DAEMON_SOURCES})

target_link_libraries(sasayaku-daemon
    sasayaku_core
    sasayaku_ai
    sasayaku_utils
    whisper
    ggml
    ${GTK4_LIBRARIES}
    ${GIO_LIBRARIES}
    ${PIPEWIRE_LIBRARIES}
    ${SPA_LIBRARIES}
    ${CURL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    m
)

if(LIBADWAITA_FOUND)
    target_link_libraries(sasayaku-daemon ${LIBADWAITA_LIBRARIES})
endif()

# Toggle executable
add_executable(sasayaku-toggle ${TOGGLE_SOURCES})
target_link_libraries(sasayaku-toggle
    ${GIO_LIBRARIES}
)

# Tray helper executable (GTK3 only, no GTK4)
add_executable(sasayaku-tray ${TRAY_HELPER_SOURCES})
set_target_properties(sasayaku-tray PROPERTIES
    INCLUDE_DIRECTORIES ""
)
target_include_directories(sasayaku-tray SYSTEM PRIVATE
    ${GTK3_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${APPINDICATOR_INCLUDE_DIRS}
)
target_link_libraries(sasayaku-tray
    ${GTK3_LIBRARIES}
    ${GIO_LIBRARIES}
    ${APPINDICATOR_LIBRARIES}
)

# Install targets
install(TARGETS sasayaku-daemon sasayaku-toggle sasayaku-tray
    RUNTIME DESTINATION bin
)

# Install data files
install(FILES data/default_modes.json
    DESTINATION share/sasayaku
)
