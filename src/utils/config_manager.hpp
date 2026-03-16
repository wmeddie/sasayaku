#pragma once

#include "../core/common.hpp"
#include "../core/whisper_wrapper.hpp"
#include "../core/audio_capture.hpp"
#include "../ai/openai_client.hpp"
#include <string>
#include <map>
#include <optional>

namespace sasayaku {

struct ModeConfig {
    std::string name;
    std::string description;
    bool use_ai;
    std::string prompt;              // System message
    std::string user_template = "{transcript}";  // User message template
    std::vector<std::string> auto_apps;  // App IDs/class names
    bool requires_clipboard = false;
};

struct Config {
    OpenAIConfig api;
    WhisperConfig whisper;
    AudioCaptureConfig audio;
    std::map<std::string, ModeConfig> modes;
    std::map<std::string, std::string> vocabulary;  // word -> replacement
    std::string default_mode = "voice_to_text";
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // Load configuration from file
    bool load();

    // Save configuration to file
    bool save();

    // Get configuration
    const Config& get_config() const { return config_; }
    Config& get_mutable_config() { return config_; }

    // Get specific mode
    std::optional<ModeConfig> get_mode(const std::string& mode_id) const;

    // Update mode
    void set_mode(const std::string& mode_id, const ModeConfig& mode);

    // Delete mode
    void delete_mode(const std::string& mode_id);

    // Get config file path
    std::string get_config_path() const;

    // Get data directory
    std::string get_data_dir() const;

    // Initialize default config if it doesn't exist
    void initialize_defaults();

private:
    Config config_;
    std::string config_path_;

    bool load_json(const std::string& path);
    bool save_json(const std::string& path);
};

} // namespace sasayaku
