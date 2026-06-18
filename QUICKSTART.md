# Sasayaku Quick Start Guide

## 🎉 Build Successful!

Your Sasayaku voice dictation app has been built successfully!

**Binaries:**
- `build/sasayaku-daemon` - Main application with UI
- `build/sasayaku-toggle` - Toggle recording (for keyboard shortcuts)

## Next Steps

### 1. Whisper Model

**You already have the model!** ✅

The application is configured to use:
- `~/.local/share/sasayaku/models/ggml-large-v3-turbo.bin` (1.6GB)
- This is Whisper's large-v3-turbo model - excellent accuracy with good speed

**Alternative models** (if you want to try different ones):
```bash
cd ~/.local/share/sasayaku/models

# Faster but less accurate options:
# wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin  # 75MB - fastest
# wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin  # 157MB - good balance
# wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin # 466MB - better accuracy

# Then update the model_path in ~/.config/sasayaku/config.json
```

### 2. Configure the Application

The application now has a **Settings UI** - no need to manually edit JSON files!

When you first start the daemon:

```bash
cd ~/Projects/github/sasayaku
./build/sasayaku-daemon
```

Click the **⚙️ Settings** button in the window to configure:

**Required settings:**
- **API Base URL**: `https://api.openai.com/v1` (or `http://localhost:8080/v1` for local llama.cpp)
- **API Key**: Your OpenAI API key (or leave empty for local llama.cpp)
- **Model**: `gpt-4o-mini` (or any model name your API supports)
- **Whisper Model Path**: Should auto-populate with `~/.local/share/sasayaku/models/ggml-large-v3-turbo.bin`
- **Use GPU**: Enable for Vulkan GPU acceleration (recommended)

**For local llama.cpp (no API costs):**
- Base URL: `http://localhost:8080/v1`
- API Key: (leave empty)
- Model: `gpt-3.5-turbo` (or whatever your local model expects)

Click **Save Settings** and the configuration will be applied immediately!

### 3. Install ydotool (for auto-paste)

```bash
sudo apt install ydotool
sudo systemctl enable --now ydotoold
```

### 4. Set Up Keyboard Shortcut

**GNOME Settings → Keyboard → View and Customize Shortcuts → Custom Shortcuts:**

1. Click "+" to add new shortcut
2. **Name:** Toggle Sasayaku Recording
3. **Command:** `/home/egonzalez/Projects/github/sasayaku/build/sasayaku-toggle`
4. **Shortcut:** Press `Super+Space` (or your preference)

### 5. Start the Daemon

```bash
cd ~/Projects/github/sasayaku
./build/sasayaku-daemon
```

The daemon will:
- Register D-Bus service `org.sasayaku.Daemon`
- Load whisper model with Vulkan GPU acceleration
- Wait for recording commands
- Show this message: "Use 'sasayaku-cli toggle-recording' to start/stop recording"

### 6. Test It!

**Method 1: Using the UI**
1. Click the "🎤 Start Recording" button in the window
2. Speak something
3. Click "⏹️  Stop Recording"
4. Wait for transcription to appear in the window
5. Text is automatically copied to clipboard - paste with Ctrl+V

**Method 2: Using Keyboard Shortcut**
1. Press your configured hotkey (e.g., Super+Space)
2. Speak something
3. Press the hotkey again to stop
4. The transcription appears in the window and is copied to clipboard

## Testing the Keyboard Shortcut

You can test the toggle command from the terminal:

```bash
# Terminal 1: Run daemon
./build/sasayaku-daemon

# Terminal 2: Toggle recording
./build/sasayaku-toggle  # Start recording
# ... speak ...
./build/sasayaku-toggle  # Stop recording
```

The UI window will update to show recording status and display the transcription.

## Modes Available

Sasayaku supports different AI-enhanced modes:

- **voice_to_text** - Simple transcription (no AI)
- **email** - Formats as professional email
- **note** - Converts to bullet-point notes
- **prompt** - Creates AI prompts
- **super** - Uses clipboard content as context
- **code** - Generates/explains code

The mode can be set in the config file's `default_mode` field.

## Troubleshooting

### Daemon won't start
- **Check whisper model path** in config.json
- **Verify Vulkan GPU:** `vulkaninfo --summary` (or check daemon logs for `ggml_vulkan: Found ... Vulkan devices`)
- **Check logs:** Run daemon in terminal to see errors

### No transcription output
- **Verify microphone access:** GNOME Settings → Privacy → Microphone
- **Check PipeWire:** `systemctl --user status pipewire`
- **Test microphone:** `parecord --channels=1 test.wav` (Ctrl+C after a few seconds, then `paplay test.wav`)

### Auto-paste not working
- **Install ydotool:** `sudo apt install ydotool`
- **Start service:** `sudo systemctl enable --now ydotoold`
- **Check status:** `sudo systemctl status ydotoold`

### API errors
- **Verify API key** in config.json
- **Test connection:** `curl https://api.openai.com/v1/models -H "Authorization: Bearer YOUR_API_KEY"`
- **For local llama.cpp:** Make sure server is running on http://localhost:8080

## What's Working

✅ Whisper.cpp transcription with Vulkan GPU acceleration
✅ PipeWire audio capture
✅ OpenAI API integration (custom base URL supported)
✅ Multiple AI-enhanced modes
✅ D-Bus IPC for keyboard shortcuts
✅ CLI control tool (sasayaku-toggle)
✅ Vocabulary mapping
✅ App-aware mode switching
✅ Config system with GUI settings window
✅ Recording window with mode dropdown and audio level visualization

## Known Limitations

- No system tray icon (GTK4 doesn't support GtkStatusIcon)
- No recording visualization (only audio level bar)
- Window tracking limited on Wayland

## Next Steps for Development

1. Add a recording window with waveform visualization
2. Create a settings UI using GTK4
3. Implement better Wayland window tracking
4. Add streaming transcription
5. Meeting mode with speaker identification
6. History and transcript editing

## Performance Tips

- **Model selection:**
  - `ggml-large-v3-turbo.bin` - Excellent accuracy, good speed (1.6GB) ⭐ **Currently using**
  - `ggml-tiny.en.bin` - Fastest (75MB)
  - `ggml-base.en.bin` - Good balance (157MB)
  - `ggml-small.en.bin` - More accurate (466MB)

- **GPU acceleration:** Enabled by default in config, uses Vulkan
- **Large-v3-turbo notes:** This is the latest turbo model - optimized for both speed and accuracy

- **API costs:** Use local llama.cpp to avoid API costs

## Support

For issues, check:
1. Terminal output from daemon
2. Config file: `~/.config/sasayaku/config.json`
3. README.md for full documentation

---

**Enjoy your new voice dictation superpowers! 🎤✨**
