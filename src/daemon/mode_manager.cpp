#include "mode_manager.hpp"
#include "../utils/clipboard.hpp"
#include <iostream>

namespace sasayaku {

ModeManager::ModeManager() {
    ai_client_ = std::make_unique<OpenAIClient>();
}

ModeManager::~ModeManager() {
}

void ModeManager::initialize(ConfigManager* config_manager) {
    config_manager_ = config_manager;

    // Configure AI client
    ai_client_->configure(config_manager->get_config().api);

    // Set default mode
    current_mode_ = config_manager->get_config().default_mode;
}

void ModeManager::set_current_mode(const std::string& mode_id) {
    if (config_manager_->get_mode(mode_id).has_value()) {
        current_mode_ = mode_id;
        std::cout << "Switched to mode: " << mode_id << std::endl;
    }
}

std::string ModeManager::get_mode_for_app(const std::string& app_id) {
    // Check each mode to see if it has this app in its auto_apps list
    const auto& modes = config_manager_->get_config().modes;

    for (const auto& [mode_id, mode] : modes) {
        for (const auto& auto_app : mode.auto_apps) {
            if (app_id.find(auto_app) != std::string::npos) {
                return mode_id;
            }
        }
    }

    // Return default mode if no match
    return config_manager_->get_config().default_mode;
}

std::string ModeManager::apply_vocabulary(const std::string& text) {
    std::string result = text;
    const auto& vocabulary = config_manager_->get_config().vocabulary;

    // Apply each vocabulary replacement
    for (const auto& [from, to] : vocabulary) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    return result;
}

std::string ModeManager::process_transcript(
    const std::string& transcript,
    const std::string& clipboard_content
) {
    // Apply vocabulary replacements first
    std::string processed_transcript = apply_vocabulary(transcript);

    // Get current mode config
    auto mode_opt = config_manager_->get_mode(current_mode_);
    if (!mode_opt.has_value()) {
        // If mode doesn't exist, return transcript as-is
        return processed_transcript;
    }

    const ModeConfig& mode = mode_opt.value();

    // If mode doesn't use AI, return transcript directly
    if (!mode.use_ai) {
        return processed_transcript;
    }

    // Build prompt from template
    std::map<std::string, std::string> template_vars;
    template_vars["transcript"] = processed_transcript;

    // Add clipboard content if this mode requires it
    if (mode.requires_clipboard && !clipboard_content.empty()) {
        template_vars["clipboard"] = clipboard_content;
    }

    std::string prompt = PromptTemplates::render(mode.prompt, template_vars);

    // Call AI client
    std::string error;
    std::string result = ai_client_->complete(prompt, &error);

    if (!error.empty()) {
        std::cerr << "AI processing error: " << error << std::endl;
        // Return original transcript if AI fails
        return processed_transcript;
    }

    return result;
}

} // namespace sasayaku
