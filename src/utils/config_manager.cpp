#include "config_manager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

using json = nlohmann::json;

namespace sasayaku {

ConfigManager::ConfigManager() {
    // Get config path
    const char* home_env = getenv("HOME");
    std::string home;

    if (home_env) {
        home = home_env;
    } else {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }

    if (!home.empty()) {
        config_path_ = home + "/.config/sasayaku/config.json";
    }
}

ConfigManager::~ConfigManager() {
}

std::string ConfigManager::get_config_path() const {
    return config_path_;
}

std::string ConfigManager::get_data_dir() const {
    const char* home_env = getenv("HOME");
    if (home_env) {
        return std::string(home_env) + "/.local/share/sasayaku";
    }
    return "";
}

void ConfigManager::initialize_defaults() {
    // Set default API config
    config_.api.base_url = "https://api.openai.com/v1";
    config_.api.model = "gpt-4o-mini";
    config_.api.temperature = 0.7f;
    config_.api.max_tokens = 2048;
    config_.api.timeout_seconds = 30;

    // Set default whisper config
    config_.whisper.model_path = get_data_dir() + "/models/ggml-large-v3-turbo.bin";
    config_.whisper.use_gpu = true;
    config_.whisper.gpu_device = 0;
    config_.whisper.n_threads = 4;
    config_.whisper.translate = false;
    config_.whisper.language = "en";
    config_.whisper.temperature = 0.0f;

    // Set default audio config
    config_.audio.source = AudioSource::MICROPHONE;
    config_.audio.sample_rate = WHISPER_SAMPLE_RATE;
    config_.audio.channels = CHANNELS;

    // Set default mode
    config_.default_mode = "voice_to_text";

    // Load default modes from system data dir
    // This would normally load from /usr/share/sasayaku/default_modes.json
    // For now, we'll define them inline

    // Voice to text mode
    ModeConfig voice_mode;
    voice_mode.name = "Voice to Text";
    voice_mode.description = "Simple transcription";
    voice_mode.use_ai = false;
    config_.modes["voice_to_text"] = voice_mode;

    // Email mode
    ModeConfig email_mode;
    email_mode.name = "Email Mode";
    email_mode.description = "Format as professional email";
    email_mode.use_ai = true;
    email_mode.prompt = "Format the following speech as a professional email:\n\n{transcript}";
    email_mode.auto_apps = {"thunderbird", "evolution", "geary"};
    config_.modes["email"] = email_mode;

    // Note mode
    ModeConfig note_mode;
    note_mode.name = "Note Mode";
    note_mode.description = "Format as bullet-point notes";
    note_mode.use_ai = true;
    note_mode.prompt = "Convert the following speech into organized bullet-point notes:\n\n{transcript}";
    note_mode.auto_apps = {"org.gnome.Notes", "gnote"};
    config_.modes["note"] = note_mode;
}

bool ConfigManager::load() {
    if (config_path_.empty()) {
        return false;
    }

    return load_json(config_path_);
}

bool ConfigManager::save() {
    if (config_path_.empty()) {
        return false;
    }

    return save_json(config_path_);
}

bool ConfigManager::load_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        // File doesn't exist, use defaults
        initialize_defaults();
        return false;
    }

    try {
        json j;
        file >> j;

        // Load API config
        if (j.contains("api")) {
            auto& api = j["api"];
            if (api.contains("base_url")) config_.api.base_url = api["base_url"];
            if (api.contains("api_key")) config_.api.api_key = api["api_key"];
            if (api.contains("model")) config_.api.model = api["model"];
            if (api.contains("temperature")) config_.api.temperature = api["temperature"];
            if (api.contains("max_tokens")) config_.api.max_tokens = api["max_tokens"];
        }

        // Load whisper config
        if (j.contains("whisper")) {
            auto& whisper = j["whisper"];
            if (whisper.contains("model_path")) config_.whisper.model_path = whisper["model_path"];
            if (whisper.contains("use_gpu")) config_.whisper.use_gpu = whisper["use_gpu"];
            if (whisper.contains("gpu_device")) config_.whisper.gpu_device = whisper["gpu_device"];
            if (whisper.contains("n_threads")) config_.whisper.n_threads = whisper["n_threads"];
            if (whisper.contains("language")) config_.whisper.language = whisper["language"];
        }

        // Load modes
        if (j.contains("modes")) {
            auto& modes = j["modes"];
            for (auto& [key, value] : modes.items()) {
                ModeConfig mode;
                if (value.contains("name")) mode.name = value["name"];
                if (value.contains("description")) mode.description = value["description"];
                if (value.contains("use_ai")) mode.use_ai = value["use_ai"];
                if (value.contains("prompt")) mode.prompt = value["prompt"];
                if (value.contains("requires_clipboard")) mode.requires_clipboard = value["requires_clipboard"];

                if (value.contains("auto_apps")) {
                    mode.auto_apps = value["auto_apps"].get<std::vector<std::string>>();
                }

                config_.modes[key] = mode;
            }
        }

        // Load vocabulary
        if (j.contains("vocabulary")) {
            config_.vocabulary = j["vocabulary"].get<std::map<std::string, std::string>>();
        }

        if (j.contains("default_mode")) {
            config_.default_mode = j["default_mode"];
        }

        return true;

    } catch (const json::exception& e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
        initialize_defaults();
        return false;
    }
}

bool ConfigManager::save_json(const std::string& path) {
    // Create directory if it doesn't exist
    std::string dir = path.substr(0, path.find_last_of('/'));
    mkdir(dir.c_str(), 0755);

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    try {
        json j;

        // Save API config
        j["api"]["base_url"] = config_.api.base_url;
        j["api"]["api_key"] = config_.api.api_key;
        j["api"]["model"] = config_.api.model;
        j["api"]["temperature"] = config_.api.temperature;
        j["api"]["max_tokens"] = config_.api.max_tokens;

        // Save whisper config
        j["whisper"]["model_path"] = config_.whisper.model_path;
        j["whisper"]["use_gpu"] = config_.whisper.use_gpu;
        j["whisper"]["gpu_device"] = config_.whisper.gpu_device;
        j["whisper"]["n_threads"] = config_.whisper.n_threads;
        j["whisper"]["language"] = config_.whisper.language;

        // Save modes
        json modes_json = json::object();
        for (const auto& [key, mode] : config_.modes) {
            json mode_json;
            mode_json["name"] = mode.name;
            mode_json["description"] = mode.description;
            mode_json["use_ai"] = mode.use_ai;
            mode_json["prompt"] = mode.prompt;
            mode_json["auto_apps"] = mode.auto_apps;
            mode_json["requires_clipboard"] = mode.requires_clipboard;
            modes_json[key] = mode_json;
        }
        j["modes"] = modes_json;

        // Save vocabulary
        j["vocabulary"] = config_.vocabulary;
        j["default_mode"] = config_.default_mode;

        file << j.dump(2);
        return true;

    } catch (const json::exception& e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
        return false;
    }
}

std::optional<ModeConfig> ConfigManager::get_mode(const std::string& mode_id) const {
    auto it = config_.modes.find(mode_id);
    if (it != config_.modes.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ConfigManager::set_mode(const std::string& mode_id, const ModeConfig& mode) {
    config_.modes[mode_id] = mode;
}

void ConfigManager::delete_mode(const std::string& mode_id) {
    config_.modes.erase(mode_id);
}

} // namespace sasayaku
