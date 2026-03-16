#pragma once

#include <string>
#include <functional>
#include <map>

namespace sasayaku {

struct OpenAIConfig {
    std::string base_url = "https://api.openai.com/v1";
    std::string api_key;
    std::string model = "gpt-4o-mini";
    float temperature = 0.7f;
    int max_tokens = 2048;
    int timeout_seconds = 30;
};

struct ChatMessage {
    std::string role;  // "system", "user", "assistant"
    std::string content;
};

using StreamCallback = std::function<void(const std::string& chunk)>;

class OpenAIClient {
public:
    OpenAIClient();
    ~OpenAIClient();

    // Configure client
    void configure(const OpenAIConfig& config);

    // Simple completion request
    std::string complete(
        const std::string& prompt,
        std::string* error = nullptr
    );

    // Chat completion with message history
    std::string chat_complete(
        const std::vector<ChatMessage>& messages,
        std::string* error = nullptr
    );

    // Streaming completion
    bool complete_stream(
        const std::string& prompt,
        StreamCallback callback,
        std::string* error = nullptr
    );

    // Test connection
    bool test_connection(std::string* error = nullptr);

private:
    OpenAIConfig config_;

    std::string make_request(
        const std::string& endpoint,
        const std::string& json_body,
        std::string* error
    );

    std::string build_chat_json(const std::vector<ChatMessage>& messages);
};

} // namespace sasayaku
