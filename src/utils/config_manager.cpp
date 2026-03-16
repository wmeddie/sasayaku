#include "config_manager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#endif

using json = nlohmann::json;

namespace sasayaku {

ConfigManager::ConfigManager() {
#ifdef _WIN32
    // Use %APPDATA%\Sasayaku
    const char* appdata = getenv("APPDATA");
    if (appdata) {
        config_path_ = std::string(appdata) + "\\Sasayaku\\config.json";
    }
#else
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
#ifdef __APPLE__
        config_path_ = home + "/Library/Application Support/Sasayaku/config.json";
#else
        config_path_ = home + "/.config/sasayaku/config.json";
#endif
    }
#endif
}

ConfigManager::~ConfigManager() {
}

std::string ConfigManager::get_config_path() const {
    return config_path_;
}

std::string ConfigManager::get_data_dir() const {
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "\\Sasayaku";
    }
#else
    const char* home_env = getenv("HOME");
    if (home_env) {
#ifdef __APPLE__
        return std::string(home_env) + "/Library/Application Support/Sasayaku";
#else
        return std::string(home_env) + "/.local/share/sasayaku";
#endif
    }
#endif
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

    // Default modes — system prompt + user_template pattern

    // Voice to text (no AI)
    ModeConfig voice_mode;
    voice_mode.name = "Voice to Text";
    voice_mode.description = "Simple transcription without formatting";
    voice_mode.use_ai = false;
    config_.modes["voice_to_text"] = voice_mode;

    // Email mode
    ModeConfig email_mode;
    email_mode.name = "Email Mode";
    email_mode.description = "Format speech as a professional email";
    email_mode.use_ai = true;
    email_mode.prompt = "You are a professional email writing assistant. Take the user's spoken words and convert them into a well-formatted, professional email. Fix grammar, add appropriate greeting and closing, and maintain a professional tone. Output only the formatted email.";
#if defined(__APPLE__)
    email_mode.auto_apps = {"com.apple.mail", "com.microsoft.Outlook", "com.readdle.smartemail"};
#elif defined(_WIN32)
    email_mode.auto_apps = {"outlook.exe", "thunderbird.exe"};
#else
    email_mode.auto_apps = {"thunderbird", "evolution", "geary"};
#endif
    config_.modes["email"] = email_mode;

    // Note mode
    ModeConfig note_mode;
    note_mode.name = "Note Mode";
    note_mode.description = "Format speech as organized bullet-point notes";
    note_mode.use_ai = true;
    note_mode.prompt = "You are a note-taking assistant. Convert the user's spoken words into well-organized bullet-point notes. Group related ideas, fix grammar, and use markdown formatting. Output only the formatted notes.";
#if defined(__APPLE__)
    note_mode.auto_apps = {"com.apple.Notes", "net.shinyfrog.bear"};
#elif defined(_WIN32)
    note_mode.auto_apps = {"notepad.exe", "onenote.exe"};
#else
    note_mode.auto_apps = {"org.gnome.Notes", "gnote"};
#endif
    config_.modes["note"] = note_mode;

    // Prompt mode
    ModeConfig prompt_mode;
    prompt_mode.name = "Prompt Mode";
    prompt_mode.description = "Create a detailed AI prompt from your speech";
    prompt_mode.use_ai = true;
    prompt_mode.prompt = "You are an AI prompt engineering expert. Convert the user's spoken description into a clear, detailed, well-structured prompt for an AI assistant. Output only the refined prompt.";
    config_.modes["prompt"] = prompt_mode;

    // Super mode (uses clipboard)
    ModeConfig super_mode;
    super_mode.name = "Super Mode";
    super_mode.description = "Process speech using clipboard content as context";
    super_mode.use_ai = true;
    super_mode.prompt = "You are a versatile AI assistant. The user will provide clipboard content and a voice command. Use the clipboard content as context to fulfill their request. Be concise and output only what was requested.";
    super_mode.user_template = "Clipboard content:\n{clipboard}\n\nRequest:\n{transcript}";
    super_mode.requires_clipboard = true;
    config_.modes["super"] = super_mode;

    // Code mode
    ModeConfig code_mode;
    code_mode.name = "Code Mode";
    code_mode.description = "Generate or explain code from speech";
    code_mode.use_ai = true;
    code_mode.prompt = "You are a coding assistant. Convert the user's spoken description into clean, well-commented code or a clear technical explanation. Use proper formatting and best practices. Output only the code or explanation.";
#if defined(__APPLE__)
    code_mode.auto_apps = {"com.microsoft.VSCode", "dev.zed.Zed", "com.todesktop.230313mzl4w4u92"};
#elif defined(_WIN32)
    code_mode.auto_apps = {"code.exe", "cursor.exe", "devenv.exe"};
#else
    code_mode.auto_apps = {"code", "cursor", "vscode", "codium", "vim", "neovim", "emacs"};
#endif
    config_.modes["code"] = code_mode;
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
                if (value.contains("user_template")) mode.user_template = value["user_template"];
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
    std::filesystem::create_directories(dir);

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
            mode_json["user_template"] = mode.user_template;
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
