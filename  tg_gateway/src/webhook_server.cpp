#include "webhook_server.hpp"
#include "parser.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>

WebhookServer::WebhookServer(const Config& config,
                              TelegramClient& tg_client,
                              RedisBus& redis,
                              const Authenticator& auth,
                              RateLimiter& rate_limiter)
    : config_(config)
    , tg_client_(tg_client)
    , redis_(redis)
    , auth_(auth)
    , rate_limiter_(rate_limiter)
    , server_(std::make_unique<httplib::Server>())
{}

void WebhookServer::start() {
    if (running_) return;
    
    setup_routes();
    running_ = true;
    
    server_thread_ = std::thread([this]() {
        spdlog::info("Starting HTTP server on {}:{}", 
                     config_.listen_addr, config_.listen_port);
        server_->listen(config_.listen_addr.c_str(), config_.listen_port);
    });
    
    spdlog::info("Webhook server started");
}

void WebhookServer::stop() {
    if (!running_) return;
    
    running_ = false;
    server_->stop();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    spdlog::info("Webhook server stopped");
}

void WebhookServer::setup_routes() {
    server_->Post("/telegram/webhook", 
        [this](const httplib::Request& req, httplib::Response& res) {
            handle_webhook(req, res);
        });
    
    server_->Get("/health",
        [this](const httplib::Request& req, httplib::Response& res) {
            handle_health(req, res);
        });
}

void WebhookServer::handle_webhook(const httplib::Request& req, httplib::Response& res) {
    try {
        auto update = nlohmann::json::parse(req.body);
        
        if (!update.contains("message")) {
            res.status = 200;
            return;
        }
        
        const auto& msg = update["message"];
        if (!msg.contains("text") || !msg.contains("from") || !msg.contains("chat")) {
            res.status = 200;
            return;
        }
        
        int64_t chat_id = msg["chat"]["id"].get<int64_t>();
        int64_t user_id = msg["from"]["id"].get<int64_t>();
        std::string text = msg["text"].get<std::string>();
        
        // Rate limiting
        if (!rate_limiter_.check_and_record(user_id)) {
            tg_client_.send_message(chat_id, "⚠️ Rate limit exceeded.");
            res.status = 200;
            return;
        }
        
        // Parse and handle similar to poller
        auto parsed = CommandParser::parse(text);
        if (!parsed.is_valid()) {
            res.status = 200;
            return;
        }
        
        // Authenticate
        auto auth_result = auth_.authenticate(user_id);
        
        if (!auth_result.authorized) {
            tg_client_.send_message(chat_id, "Access denied.");
            res.status = 200;
            return;
        }
        
        // Publish command
        std::string corr_id = util::generate_uuid();
        auto request_json = CommandParser::to_request_json(
            parsed, user_id, auth_result.role_string(), corr_id);
        
        redis_.publish_request(config_.stream_req, request_json);
        
        res.status = 200;
        
    } catch (const std::exception& e) {
        spdlog::error("Webhook handler error: {}", e.what());
        res.status = 500;
    }
}

void WebhookServer::handle_health(const httplib::Request& req, httplib::Response& res) {
    bool redis_ok = redis_.ping();
    
    nlohmann::json health = {
        {"ok", redis_ok},
        {"redis", redis_ok},
        {"mode", config_.gateway_mode}
    };
    
    res.set_content(health.dump(), "application/json");
    res.status = redis_ok ? 200 : 503;
}