#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
WHISPER_DIR="$PROJECT_ROOT/whisper.cpp"

echo "=== Sasayaku macOS Build ==="
echo ""

# Step 1: Build whisper.cpp for macOS (with Metal)
if [ ! -f "$WHISPER_DIR/build/src/libwhisper.a" ]; then
    echo "--- Building whisper.cpp with Metal support ---"
    if [ ! -d "$WHISPER_DIR" ]; then
        echo "Error: whisper.cpp not found at $WHISPER_DIR"
        echo "Please clone whisper.cpp:"
        echo "  cd $PROJECT_ROOT && git clone https://github.com/ggerganov/whisper.cpp.git"
        exit 1
    fi

    mkdir -p "$WHISPER_DIR/build"
    cd "$WHISPER_DIR/build"
    cmake .. -DGGML_METAL=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build . -j$(sysctl -n hw.ncpu)
    cd "$SCRIPT_DIR"
    echo "whisper.cpp built successfully"
else
    echo "whisper.cpp already built, skipping"
fi

echo ""

# Step 2: Generate Xcode project (requires xcodegen)
if command -v xcodegen &> /dev/null; then
    echo "--- Generating Xcode project ---"
    cd "$SCRIPT_DIR"
    xcodegen generate
    echo "Xcode project generated"
else
    echo "xcodegen not found. Install with: brew install xcodegen"
    echo "Then run: cd macos && xcodegen generate"
    echo ""
    echo "Alternatively, build the C++ core with CMake:"
    echo "  mkdir -p build && cd build && cmake .. && make -j\$(sysctl -n hw.ncpu)"
    exit 1
fi

echo ""

# Step 3: Build with xcodebuild
echo "--- Building Sasayaku.app ---"
cd "$SCRIPT_DIR"
xcodebuild -project Sasayaku.xcodeproj \
    -scheme Sasayaku \
    -configuration Release \
    -derivedDataPath build/DerivedData \
    build

APP_PATH="build/DerivedData/Build/Products/Release/Sasayaku.app"
if [ -d "$APP_PATH" ]; then
    echo ""
    echo "=== Build successful ==="
    echo "App: $APP_PATH"
    echo ""
    echo "To run: open $APP_PATH"
    echo ""
    echo "Before first run, download a whisper model:"
    echo "  mkdir -p ~/Library/Application\\ Support/Sasayaku/models"
    echo "  # Download ggml-large-v3-turbo.bin to that directory"
else
    echo "Build may have succeeded but app not found at expected path."
    echo "Check build/DerivedData for output."
fi
