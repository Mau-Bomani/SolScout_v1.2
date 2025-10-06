#pragma once

#include "telegram_client.hpp"
#include "redis_bus.hpp"
#include "auth.hpp"
#include "parser.hpp"
#include "rate_limiter.hpp"
#include "config.hpp"
#include <atomic>
#include <thread>
#include <functional>

using MessageHandler = std::function<void(int64_t chat_id, int64_t user_id, const std::string& text)>;

class TelegramPoller {
public:
    TelegramPoller(TelegramClient& tg_client,
                   RedisBus& redis,
                   const Authenticator& auth,
                   RateLimiter& rate_limiter,
                   const Config& config);
    
    void start();
    void stop();
    bool is_running() const { return running_; }
    
private:
    TelegramClient& tg_client_;
    RedisBus& redis_;
    const Authenticator& auth_;
    RateLimiter& rate_limiter_;
    const Config& config_;
    
    std::atomic<bool> running_{false};
    std::thread poll_thread_;
    int last_update_id_ = 0;
    
    void poll_loop();
    void process_update(const nlohmann::json& update);
    void handle_message(int64_t chat_id, int64_t user_id, const std::string& text);
    void handle_start_command(int64_t chat_id, int64_t user_id, const ParsedCommand& cmd);
    void handle_guest_command(int64_t chat_id, int64_t user_id);
    void send_help(int64_t chat_id, UserRole role);
};
