#pragma once

#include <string>
#include <map>

namespace sasayaku {

class PromptTemplates {
public:
    // Replace placeholders in template with values
    // Placeholders are in the format {key}
    static std::string render(
        const std::string& template_str,
        const std::map<std::string, std::string>& values
    );

    // Common template helpers
    static std::string create_chat_prompt(
        const std::string& system_prompt,
        const std::string& user_message
    );

    // Escape special characters for JSON
    static std::string escape_json(const std::string& str);

    // Truncate text to max length with ellipsis
    static std::string truncate(
        const std::string& text,
        size_t max_length,
        const std::string& suffix = "..."
    );
};

} // namespace sasayaku
