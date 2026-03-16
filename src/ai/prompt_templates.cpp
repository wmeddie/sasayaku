#include "prompt_templates.hpp"
#include <sstream>
#include <algorithm>

namespace sasayaku {

std::string PromptTemplates::render(
    const std::string& template_str,
    const std::map<std::string, std::string>& values
) {
    std::string result = template_str;

    // Replace all placeholders {key} with values
    for (const auto& [key, value] : values) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;

        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    return result;
}

std::string PromptTemplates::create_chat_prompt(
    const std::string& system_prompt,
    const std::string& user_message
) {
    std::stringstream ss;
    ss << "System: " << system_prompt << "\n\n";
    ss << "User: " << user_message;
    return ss.str();
}

std::string PromptTemplates::escape_json(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c >= 0x00 && c <= 0x1f) {
                    // Control characters
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }

    return result;
}

std::string PromptTemplates::truncate(
    const std::string& text,
    size_t max_length,
    const std::string& suffix
) {
    if (text.length() <= max_length) {
        return text;
    }

    if (max_length <= suffix.length()) {
        return text.substr(0, max_length);
    }

    return text.substr(0, max_length - suffix.length()) + suffix;
}

} // namespace sasayaku
