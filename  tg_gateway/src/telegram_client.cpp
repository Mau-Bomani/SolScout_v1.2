#include "telegram_client.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

TelegramClient::TelegramClient(const std::string& bot_token)
    : bot_token_(bot_token)
    , api_base_("https://api.telegram.org/bot" + bot_token)
    , curl_(curl_easy_init())
{
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    // Set common CURL options
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 30L);
}

TelegramClient::~TelegramClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

size_t TelegramClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

nlohmann::json TelegramClient::make_request(const std::string& method, 
                                             const nlohmann::json& params) {
    std::string url = api_base_ + "/" + method;
    std::string response_string;
    
    // Build URL with params
    if (!params.empty()) {
        url += "?";
        bool first = true;
        for (auto& [key, value] : params.items()) {
            if (!first) url += "&";
            first = false;
            
            std::string val_str;
            if (value.is_string()) {
                val_str = value.get<std::string>();
            } else {
                val_str = value.dump();
            }
            
            // URL encode value
            char* escaped = curl_easy_escape(curl_, val_str.c_str(), val_str.length());
            url += key + "=" + std::string(escaped);
            curl_free(escaped);
        }
    }
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);
    
    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        spdlog::error("Telegram API request failed: {}", curl_easy_strerror(res));
        return nlohmann::json{{"ok", false}, {"error", curl_easy_strerror(res)}};
    }
    
    try {
        return nlohmann::json::parse(response_string);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse Telegram API response: {}", e.what());
        return nlohmann::json{{"ok", false}, {"error", "Parse error"}};
    }
}

bool TelegramClient::send_message(int64_t chat_id, const std::string& text) {
    nlohmann::json params = {
        {"chat_id", chat_id},
        {"text", text},
        {"parse_mode", "HTML"}
    };
    
    auto response = make_request("sendMessage", params);
    
    if (!response["ok"].get<bool>()) {
        spdlog::error("Failed to send message: {}", response.dump());
        return false;
    }
    
    return true;
}

nlohmann::json TelegramClient::get_updates(int offset, int timeout) {
    nlohmann::json params = {
        {"offset", offset},
        {"timeout", timeout}
    };
    
    return make_request("getUpdates", params);
}

bool TelegramClient::set_webhook(const std::string& url) {
    nlohmann::json params = {
        {"url", url}
    };
    
    auto response = make_request("setWebhook", params);
    return response["ok"].get<bool>();
}

bool TelegramClient::delete_webhook() {
    auto response = make_request("deleteWebhook");
    return response["ok"].get<bool>();
}