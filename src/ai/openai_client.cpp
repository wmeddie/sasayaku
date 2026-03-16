#include "openai_client.hpp"
#include "prompt_templates.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace sasayaku {

// libcurl write callback
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

OpenAIClient::OpenAIClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

OpenAIClient::~OpenAIClient() {
    curl_global_cleanup();
}

void OpenAIClient::configure(const OpenAIConfig& config) {
    config_ = config;
}

std::string OpenAIClient::complete(
    const std::string& prompt,
    std::string* error
) {
    std::vector<ChatMessage> messages = {
        {"user", prompt}
    };

    return chat_complete(messages, error);
}

std::string OpenAIClient::chat_complete(
    const std::vector<ChatMessage>& messages,
    std::string* error
) {
    std::string json_body = build_chat_json(messages);
    std::string endpoint = config_.base_url + "/chat/completions";

    std::string response = make_request(endpoint, json_body, error);

    if (response.empty()) {
        return "";
    }

    try {
        json response_json = json::parse(response);

        if (response_json.contains("error")) {
            if (error) {
                *error = "API error: " + response_json["error"]["message"].get<std::string>();
            }
            return "";
        }

        if (response_json.contains("choices") && !response_json["choices"].empty()) {
            auto& choice = response_json["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                return choice["message"]["content"].get<std::string>();
            }
        }

        if (error) {
            *error = "Unexpected response format";
        }
        return "";

    } catch (const json::exception& e) {
        if (error) {
            *error = std::string("JSON parse error: ") + e.what();
        }
        return "";
    }
}

bool OpenAIClient::complete_stream(
    const std::string& prompt,
    StreamCallback callback,
    std::string* error
) {
    // TODO: Implement streaming support
    if (error) {
        *error = "Streaming not yet implemented";
    }
    return false;
}

bool OpenAIClient::test_connection(std::string* error) {
    std::string test_prompt = "Say 'OK' if you can hear me.";
    std::string result = complete(test_prompt, error);
    return !result.empty();
}

std::string OpenAIClient::make_request(
    const std::string& endpoint,
    const std::string& json_body,
    std::string* error
) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = "Failed to initialize CURL";
        }
        return "";
    }

    std::string response;
    struct curl_slist* headers = nullptr;

    // Set headers
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + config_.api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        if (error) {
            *error = std::string("CURL error: ") + curl_easy_strerror(res);
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "";
    }

    // Check HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code < 200 || http_code >= 300) {
        if (error) {
            *error = "HTTP error " + std::to_string(http_code) + ": " + response;
        }
        return "";
    }

    return response;
}

std::string OpenAIClient::build_chat_json(const std::vector<ChatMessage>& messages) {
    json j;
    j["model"] = config_.model;
    j["temperature"] = config_.temperature;
    j["max_tokens"] = config_.max_tokens;

    json messages_array = json::array();
    for (const auto& msg : messages) {
        json msg_obj;
        msg_obj["role"] = msg.role;
        msg_obj["content"] = msg.content;
        messages_array.push_back(msg_obj);
    }
    j["messages"] = messages_array;

    return j.dump();
}

} // namespace sasayaku
