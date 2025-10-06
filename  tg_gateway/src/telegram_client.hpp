#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <memory>

class TelegramClient {
public:
    explicit TelegramClient(const std::string& bot_token);
    ~TelegramClient();
    
    // Disable copy
    TelegramClient(const TelegramClient&) = delete;
    TelegramClient& operator=(const TelegramClient&) = delete;
    
    // Send message
    bool send_message(int64_t chat_id, const std::string& text);
    
    // Get updates (for polling mode)
    nlohmann::json get_updates(int offset, int timeout = 30);
    
    // Set webhook
    bool set_webhook(const std::string& url);
    bool delete_webhook();
    
private:
    std::string bot_token_;
    std::string api_base_;
    CURL* curl_;
    
    nlohmann::json make_request(const std::string& method, 
                                 const nlohmann::json& params = {});
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};
