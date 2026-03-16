cmake_minimum_required(VERSION 3.20)
project(sasayaku-macos VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)

# Only build on macOS
if(NOT APPLE)
    message(FATAL_ERROR "This CMakeLists.txt is for macOS only. Use the top-level CMakeLists.txt for Linux.")
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

# macOS platform sources (Objective-C++)
set(MACOS_PLATFORM_SOURCES
    Sasayaku/Platform/AudioCaptureMacOS.mm
    Sasayaku/Platform/ClipboardMacOS.mm
    Sasayaku/Platform/AutoPasteMacOS.mm
    Sasayaku/Platform/WindowTrackerMacOS.mm
)

# Bridge sources
set(BRIDGE_SOURCES
    Sasayaku/Bridge/SasayakuBridge.mm
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

# Build the C++ + ObjC++ library
add_library(sasayaku_macos_core STATIC
    ${SHARED_SOURCES}
    ${MACOS_PLATFORM_SOURCES}
    ${BRIDGE_SOURCES}
)

target_compile_definitions(sasayaku_macos_core PRIVATE __APPLE__=1)

target_link_libraries(sasayaku_macos_core
    whisper
    ggml
    ${CURL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    "-framework AVFoundation"
    "-framework CoreGraphics"
    "-framework Carbon"
    "-framework AppKit"
    "-framework Accelerate"
    "-framework Metal"
    "-framework MetalKit"
)

# Note: The SwiftUI app must be built with Xcode or xcodebuild.
# This CMakeLists.txt builds the C++/ObjC++ library that the Swift app links against.
message(STATUS "")
message(STATUS "Built sasayaku_macos_core library.")
message(STATUS "To build the full macOS app, use Xcode:")
message(STATUS "  1. Install xcodegen: brew install xcodegen")
message(STATUS "  2. cd macos && xcodegen generate")
message(STATUS "  3. Open Sasayaku.xcodeproj in Xcode")
message(STATUS "")
