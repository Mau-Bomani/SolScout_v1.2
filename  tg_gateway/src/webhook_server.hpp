#pragma once

#include "config.hpp"
#include "telegram_client.hpp"
#include "redis_bus.hpp"
#include "auth.hpp"
#include "rate_limiter.hpp"
#include <httplib.h>
#include <atomic>
#include <thread>

class WebhookServer {
public:
    WebhookServer(const Config& config,
                  TelegramClient& tg_client,
                  RedisBus& redis,
                  const Authenticator& auth,
                  RateLimiter& rate_limiter);
    
    void start();
    void stop();
    bool is_running() const { return running_; }
    
private:
    const Config& config_;
    TelegramClient& tg_client_;
    RedisBus& redis_;
    const Authenticator& auth_;
    RateLimiter& rate_limiter_;
    
    std::unique_ptr<httplib::Server> server_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    
    void setup_routes();
    void handle_webhook(const httplib::Request& req, httplib::Response& res);
    void handle_health(const httplib::Request& req, httplib::Response& res);
};
