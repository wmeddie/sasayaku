# Sasayaku (囁く)

A cross-platform voice dictation app with AI-enhanced modes. Runs entirely on your machine — speech recognition via whisper.cpp and text processing via Ollama. No cloud services required. "Sasayaku" means "to whisper" in Japanese.

## Features

- **Fully local** — speech-to-text and AI processing run on your hardware, nothing leaves your machine
- **Cross-platform** — Linux (GNOME/Wayland), macOS, and Windows
- **Voice-to-text transcription** using whisper.cpp with GPU acceleration (CUDA, Metal)
- **AI-enhanced modes** — Email, Note, Prompt, Code, and custom modes with configurable system/user prompts
- **Works with Ollama** — use any local model (default: `qwen3:0.6b` for speed) or any OpenAI-compatible API
- **Floating overlay** — unobtrusive recording/processing/result panel with waveform visualization
- **Editable results** — review and fix transcription before accepting
- **App-aware mode switching** — automatically picks the right mode based on the active window
- **Clipboard integration** — Super mode uses clipboard content as context
- **Custom vocabulary** — map words and phrases for consistent spelling
- **Global hotkey** — toggle recording from anywhere (Option+Space on macOS, Alt+Space on Windows/Linux)
- **Input device selection** — choose which microphone to use

## Quick Start

### 1. Install Ollama

Download from [ollama.com](https://ollama.com) and pull a small, fast model:

```bash
ollama pull qwen3:0.6b
```

### 2. Build & Run

See platform-specific instructions below.

### 3. Download a Whisper Model

On all platforms, you can download whisper models directly from the **Settings UI** — just pick a model from the dropdown and click Download.

Available models (smaller = faster, larger = more accurate):

| Model | Size | Speed | Quality |
|-------|------|-------|---------|
| Tiny | 75 MB | Fastest | Basic |
| Base | 142 MB | Fast | Good |
| Small | 466 MB | Moderate | Better |
| Medium | 1.5 GB | Slow | Great |
| Large v3 Turbo | 1.6 GB | Moderate | Excellent |
| Large v3 | 3.1 GB | Slowest | Best |

### 4. Configure

On first run, open Settings and configure:

- **API Base URL**: `http://localhost:11434/v1/` (Ollama default)
- **API Key**: `ollama`
- **Model**: `qwen3:0.6b`
- **Whisper Model**: select and download from the model picker
- **Input Device**: choose your microphone

## Platform Setup

### macOS

**Prerequisites**: Xcode, Homebrew

```bash
# Install xcodegen
brew install xcodegen

# Clone and build
git submodule update --init --recursive
cd macos
./build.sh
```

Or manually:
```bash
# Build whisper.cpp with Metal
cd whisper.cpp && mkdir build && cd build
cmake .. -DGGML_METAL=ON -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)
cd ../../macos

# Generate Xcode project and build
xcodegen generate
xcodebuild -project Sasayaku.xcodeproj -scheme Sasayaku -configuration Release build
```

The app runs as a menu bar icon. Press **Option+Space** to toggle recording.

Config location: `~/Library/Application Support/Sasayaku/config.json`

### Linux (GNOME/Wayland)

**Prerequisites**: GTK4, libadwaita, PipeWire, Meson, CUDA toolkit (optional)

```bash
# Install dependencies (Ubuntu/Fedora)
sudo apt install build-essential meson pkg-config cmake
sudo apt install libgtk-4-dev libadwaita-1-dev libgio-2.0-dev
sudo apt install libpipewire-0.3-dev libspa-0.2-dev
sudo apt install libcurl4-openssl-dev nlohmann-json3-dev
sudo apt install ydotool  # Optional: for auto-pasting

# Build whisper.cpp
git submodule update --init --recursive
cd whisper.cpp && mkdir build && cd build
cmake .. -DGGML_CUDA=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build . -j$(nproc)
cd ../..

# Build Sasayaku
meson setup build
meson compile -C build
./build/sasayaku-daemon
```

Set up a keyboard shortcut in GNOME Settings pointing to `sasayaku-toggle`.

Config location: `~/.config/sasayaku/config.json`

### Windows

**Prerequisites**: Visual Studio with C++ workload, CMake, curl

```bash
# Build whisper.cpp
git submodule update --init --recursive
cd whisper.cpp && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..\..

# Build Sasayaku
cd windows
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Or use the build script: `windows\build.bat`

Press **Alt+Space** to toggle recording. The app lives in the system tray.

Config location: `%APPDATA%\Sasayaku\config.json`

## Modes

Sasayaku processes transcriptions through configurable AI modes. Each mode has a **system prompt** (sets the AI's role) and a **user message template** (defaults to `{transcript}`).

### Built-in Modes

| Mode | Description |
|------|-------------|
| **Voice to Text** | Raw transcription, no AI processing |
| **Email** | Formats speech as a professional email |
| **Note** | Converts speech to organized bullet-point notes |
| **Prompt** | Turns speech into a well-structured AI prompt |
| **Super** | Uses clipboard content as context for processing |
| **Code** | Generates or explains code from speech |

### Custom Modes

Create any mode you need in Settings > Modes. Examples:

**Business Japanese translator**:
- System: `You are a translator. Translate the user's spoken English into formal business Japanese (敬語). Output only the Japanese translation.`
- User: `{transcript}`

**Git commit message**:
- System: `You write concise git commit messages. Given a description of changes, output a conventional commit message.`
- User: `{transcript}`

### App-Aware Switching

Modes can auto-activate based on the foreground app. Configure `auto_apps` in the config file or the Modes editor.

## Architecture

```
┌──────────────────────────────────────────────────┐
│                  Shared C++ Core                 │
│  whisper.cpp │ Audio Processor │ OpenAI Client   │
│  Mode Manager │ Recording Coordinator │ Config   │
└──────────────────┬───────────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
┌───┴───┐    ┌────┴────┐   ┌────┴─────┐
│ Linux │    │  macOS  │   │ Windows  │
│ GTK4  │    │ SwiftUI │   │  Win32   │
│PipeWire│   │AVAudio  │   │ WASAPI   │
│ D-Bus │    │ Carbon  │   │ GDI+     │
└───────┘    └─────────┘   └──────────┘
```

The core transcription engine, AI client, mode manager, and config system are shared C++17 code. Each platform provides:

- **Audio capture** — PipeWire (Linux), AVAudioEngine (macOS), WASAPI (Windows)
- **Clipboard** — GTK (Linux), NSPasteboard (macOS), Win32 (Windows)
- **Auto-paste** — ydotool (Linux), CGEvent (macOS), SendInput (Windows)
- **Window tracking** — GNOME Shell (Linux), NSWorkspace (macOS), Win32 (Windows)
- **UI** — GTK4 (Linux), SwiftUI + floating NSPanel (macOS), GDI+ overlay (Windows)

## Configuration

### Example config

```json
{
  "api": {
    "base_url": "http://localhost:11434/v1/",
    "api_key": "ollama",
    "model": "qwen3:0.6b",
    "temperature": 0.7,
    "max_tokens": 2048
  },
  "whisper": {
    "model_path": "/path/to/ggml-small.bin",
    "use_gpu": true,
    "language": "en"
  },
  "modes": {
    "email": {
      "name": "Email Mode",
      "use_ai": true,
      "prompt": "You are a professional email writer...",
      "user_template": "{transcript}",
      "auto_apps": ["thunderbird", "outlook.exe", "com.apple.mail"]
    }
  },
  "vocabulary": {
    "API": "API",
    "Kubernetes": "Kubernetes"
  },
  "default_mode": "voice_to_text"
}
```

### Template Variables

- `{transcript}` — the transcribed speech
- `{clipboard}` — current clipboard content (for modes with `requires_clipboard: true`)

## License

GPL-3.0 — see [LICENSE](LICENSE) for details.

## Credits

- [whisper.cpp](https://github.com/ggerganov/whisper.cpp) by Georgi Gerganov
- [Ollama](https://ollama.com) for local LLM inference
