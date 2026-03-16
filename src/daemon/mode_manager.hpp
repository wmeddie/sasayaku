#pragma once

#include "../utils/config_manager.hpp"
#include "../ai/openai_client.hpp"
#include "../ai/prompt_templates.hpp"
#include <string>
#include <memory>

namespace sasayaku {

class ModeManager {
public:
    ModeManager();
    ~ModeManager();

    // Initialize with config
    void initialize(ConfigManager* config_manager);

    // Get current mode
    std::string get_current_mode() const { return current_mode_; }

    // Set current mode
    void set_current_mode(const std::string& mode_id);

    // Get mode for application
    std::string get_mode_for_app(const std::string& app_id);

    // Process transcript with current mode
    std::string process_transcript(
        const std::string& transcript,
        const std::string& clipboard_content = ""
    );

    // Apply vocabulary replacements
    std::string apply_vocabulary(const std::string& text);

private:
    ConfigManager* config_manager_ = nullptr;
    std::unique_ptr<OpenAIClient> ai_client_;
    std::string current_mode_;
};

} // namespace sasayaku
