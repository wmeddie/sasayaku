cmake_minimum_required(VERSION 3.20)
project(sasayaku-windows VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This CMakeLists.txt is for Windows only. Use the top-level CMakeLists.txt for Linux.")
endif()

# whisper.cpp
set(WHISPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../whisper.cpp")
set(WHISPER_BUILD_DIR "${WHISPER_DIR}/build")

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# Shared C++ core sources (platform-independent)
set(SHARED_SOURCES
    ../src/core/audio_processor.cpp
    ../src/core/whisper_wrapper.cpp
    ../src/core/transcription.cpp
    ../src/ai/openai_client.cpp
    ../src/ai/prompt_templates.cpp
    ../src/daemon/mode_manager.cpp
    ../src/daemon/recording_coordinator.cpp
    ../src/utils/config_manager.cpp
)

# Windows platform sources
set(WINDOWS_PLATFORM_SOURCES
    Platform/AudioCaptureWindows.cpp
    Platform/ClipboardWindows.cpp
    Platform/AutoPasteWindows.cpp
    Platform/WindowTrackerWindows.cpp
)

# Windows app sources
set(WINDOWS_APP_SOURCES
    App/main.cpp
    App/OverlayWindow.cpp
    App/HotkeyManager.cpp
    App/TrayIcon.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    ${WHISPER_DIR}/include
    ${WHISPER_DIR}/ggml/include
    ${CURL_INCLUDE_DIRS}
)

# Link directories
link_directories(
    ${WHISPER_BUILD_DIR}/src
    ${WHISPER_BUILD_DIR}/ggml/src
)

# Main executable
add_executable(sasayaku WIN32
    ${SHARED_SOURCES}
    ${WINDOWS_PLATFORM_SOURCES}
    ${WINDOWS_APP_SOURCES}
)

target_compile_definitions(sasayaku PRIVATE _WIN32=1 UNICODE _UNICODE)

target_link_libraries(sasayaku
    whisper
    ggml
    ${CURL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ole32
    oleaut32
    user32
    gdi32
    shell32
    gdiplus
    psapi
)

# Toggle CLI tool
add_executable(sasayaku-toggle Toggle/toggle.cpp)
target_link_libraries(sasayaku-toggle kernel32)

# Install
install(TARGETS sasayaku sasayaku-toggle RUNTIME DESTINATION bin)
