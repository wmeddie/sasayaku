# Sasayaku (囁く)

A GNOME voice dictation application for Linux/Wayland inspired by SuperWhisper for macOS. "Sasayaku" means "to whisper" in Japanese.

## Features

- **Voice-to-text transcription** using whisper.cpp with CUDA acceleration
- **Multiple AI-enhanced modes**: Email, Note, Prompt, Super mode, and more
- **Intelligent text formatting** using OpenAI-compatible APIs (OpenAI, local llama.cpp, etc.)
- **App-aware mode switching**: Automatically switch modes based on the active application
- **Clipboard integration**: Super mode uses clipboard content as context
- **Recording UI**: Window with mode dropdown, audio level visualization, and transcription display
- **Settings UI**: Configure API keys, models, and Whisper settings through a GUI
- **Custom vocabulary**: Map words and phrases for consistent spelling
- **Keyboard shortcut support**: Toggle recording with custom GNOME shortcuts
- **Wayland native**: Built for modern Linux desktop environments

## Prerequisites

- Ubuntu 25.10 or similar (Wayland)
- NVIDIA GPU with CUDA support
- GTK4 and libadwaita
- PipeWire for audio capture
- Meson build system

### Install Dependencies

```bash
sudo apt install build-essential meson pkg-config cmake cuda-toolkit
sudo apt install libgtk-4-dev libadwaita-1-dev libgio-2.0-dev
sudo apt install libpipewire-0.3-dev libspa-0.2-dev
sudo apt install libcurl4-openssl-dev nlohmann-json3-dev
sudo apt install ydotool  # For auto-pasting
```

## Building

1. **Build whisper.cpp with CUDA** (already done in this repo):
```bash
cd whisper.cpp
mkdir -p build && cd build
cmake .. -DGGML_CUDA=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build . -j$(nproc)
cd ../..
```

2. **Build Sasayaku**:
```bash
meson setup build
meson compile -C build
```

3. **Install** (optional):
```bash
sudo meson install -C build
```

## Setup

### 1. Download a Whisper Model

Download a whisper model from the whisper.cpp repository:

```bash
# Create models directory
mkdir -p ~/.local/share/sasayaku/models

# Download large-v3-turbo model (recommended - best quality/speed tradeoff)
cd ~/.local/share/sasayaku/models
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo.bin

# Or download other models:
# ggml-tiny.en.bin    - Fastest, less accurate (75MB)
# ggml-base.en.bin    - Good balance (157MB)
# ggml-small.en.bin   - Better accuracy (466MB)
# ggml-medium.en.bin  - Even better accuracy, slower (1.5GB)
```

### 2. Configure Settings

When you first run the daemon, you can configure settings through the **Settings UI**:

1. Start the daemon: `./build/sasayaku-daemon`
2. Click the **⚙️ Settings** button
3. Configure:
   - **API Base URL**: `https://api.openai.com/v1` (or local llama.cpp URL)
   - **API Key**: Your OpenAI API key
   - **Model**: `gpt-4o-mini`
   - **Whisper Model Path**: Path to your downloaded model
   - **Use GPU**: Enable for CUDA acceleration
4. Click **Save Settings**

**For local llama.cpp:**
- Base URL: `http://localhost:8080/v1`
- API Key: (leave empty)
- Model: `gpt-3.5-turbo`

Alternatively, you can manually edit `~/.config/sasayaku/config.json`.

### 3. Set Up Keyboard Shortcut

In GNOME Settings → Keyboard → View and Customize Shortcuts → Custom Shortcuts:

1. Click "+" to add a new shortcut
2. Name: "Toggle Sasayaku Recording"
3. Command: `/usr/local/bin/sasayaku-toggle`
   (or `~/Projects/github/sasayaku/build/sasayaku-toggle` if not installed)
4. Shortcut: Press your desired key combination (e.g., Super+Space)

### 4. Start the Daemon

```bash
# If installed:
sasayaku-daemon

# Or from build directory:
./build/sasayaku-daemon
```

The daemon will:
- Show a recording window with UI controls
- Register a D-Bus service
- Load configuration from `~/.config/sasayaku/config.json`
- Wait for recording commands from UI or keyboard shortcut

### 5. Enable Auto-start (Optional)

To start the daemon automatically on login:

```bash
# If installed, the desktop file will be in /etc/xdg/autostart/
# Otherwise, copy it manually:
mkdir -p ~/.config/autostart
cp data/sasayaku-daemon.desktop ~/.config/autostart/
# Edit the Exec= line to point to your binary
```

## Usage

### Basic Recording

**Method 1: UI Window**
1. Click "🎤 Start Recording" button
2. Speak into your microphone
3. Click "⏹️  Stop Recording"
4. Text appears in the window and is copied to clipboard

**Method 2: Keyboard Shortcut**
1. Press your configured keyboard shortcut (e.g., Super+Space)
2. Speak into your microphone
3. Press the shortcut again to stop recording
4. Text appears in the UI window and is copied to clipboard

### Modes

Sasayaku supports different modes for different contexts:

- **Voice to Text**: Simple transcription without AI enhancement
- **Email Mode**: Formats speech as a professional email
- **Note Mode**: Converts speech into organized bullet-point notes
- **Prompt Mode**: Creates detailed AI prompts from your speech
- **Super Mode**: Uses clipboard content as context for processing
- **Code Mode**: Generates or explains code from speech

### App-Aware Mode Switching

Sasayaku can automatically switch modes based on the active application:

- Thunderbird/Evolution → Email mode
- Notes/Gnote → Note mode
- VS Code/Cursor → Code mode
- ChatGPT/Claude → Prompt mode

Configure app associations in `~/.config/sasayaku/config.json`:

```json
{
  "modes": {
    "email": {
      "auto_apps": ["thunderbird", "evolution", "geary"]
    }
  }
}
```

### Custom Vocabulary

Add custom word mappings in config:

```json
{
  "vocabulary": {
    "API": "API",
    "Linux": "Linux",
    "optimize": "optimise"
  }
}
```

### Toggle Command

```bash
sasayaku-toggle  # Toggle recording on/off
```

This simple command makes it easy to bind to keyboard shortcuts in GNOME.

## Configuration

Configuration file: `~/.config/sasayaku/config.json`

### Example Configuration

```json
{
  "api": {
    "base_url": "https://api.openai.com/v1",
    "api_key": "sk-...",
    "model": "gpt-4o-mini",
    "temperature": 0.7,
    "max_tokens": 2048
  },
  "whisper": {
    "model_path": "/home/user/.local/share/sasayaku/models/ggml-large-v3-turbo.bin",
    "use_gpu": true,
    "gpu_device": 0,
    "n_threads": 4,
    "language": "en"
  },
  "modes": {
    "voice_to_text": {
      "name": "Voice to Text",
      "use_ai": false
    },
    "email": {
      "name": "Email Mode",
      "use_ai": true,
      "prompt": "Format as professional email:\n\n{transcript}",
      "auto_apps": ["thunderbird", "evolution"]
    }
  },
  "vocabulary": {
    "API": "API",
    "GPT": "GPT"
  },
  "default_mode": "voice_to_text"
}
```

## Troubleshooting

### Daemon won't start

- Check if whisper model path is correct in config
- Verify CUDA is working: `nvidia-smi`
- Check dependencies are installed

### No audio capture

- Verify PipeWire is running: `systemctl --user status pipewire`
- Check microphone permissions in GNOME Settings

### Auto-paste not working

- Install ydotool: `sudo apt install ydotool`
- Start ydotool service: `sudo systemctl enable --now ydotoold`
- Grant input permissions if needed

### D-Bus errors

- Make sure only one instance of sasayaku-daemon is running
- Check D-Bus session: `gdbus introspect --session --dest org.sasayaku.Daemon --object-path /org/sasayaku/Daemon`

## Architecture

```
┌─────────────────────┐
│   User Keyboard     │
│   Shortcut          │
└──────────┬──────────┘
           │
           v
┌─────────────────────┐
│   sasayaku-cli      │ (Sends D-Bus commands)
└──────────┬──────────┘
           │
           v
┌──────────────────────────────────────────┐
│         sasayaku-daemon                  │
│  ┌────────────────────────────────────┐  │
│  │  D-Bus Service                     │  │
│  │  - Recording control               │  │
│  │  - Status queries                  │  │
│  └────────────────────────────────────┘  │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │  Recording Coordinator             │  │
│  │  - Audio capture (PipeWire)        │  │
│  │  - Transcription (whisper.cpp)     │  │
│  │  - AI processing (OpenAI API)      │  │
│  └────────────────────────────────────┘  │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │  Mode Manager                      │  │
│  │  - App-aware mode switching        │  │
│  │  - Prompt templates                │  │
│  │  - Vocabulary mapping              │  │
│  └────────────────────────────────────┘  │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │  System Tray Icon                  │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

## Future Enhancements

- [x] Recording window with audio level visualization
- [x] Settings UI window
- [ ] Meeting mode with speaker identification
- [ ] Better Wayland window tracking
- [ ] Streaming transcription
- [ ] Offline mode with local LLM
- [ ] History and editing
- [ ] System tray icon (waiting for GTK4 solution)

## License

MIT

## Credits

- Based on [whisper.cpp](https://github.com/ggerganov/whisper.cpp) by Georgi Gerganov
- Inspired by [SuperWhisper](https://superwhisper.com/) for macOS
# sasayaku
