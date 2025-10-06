#include "poller.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

TelegramPoller::TelegramPoller(TelegramClient& tg_client,
                                RedisBus& redis,
                                const Authenticator& auth,
                                RateLimiter& rate_limiter,
                                const Config& config)
    : tg_client_(tg_client)
    , redis_(redis)
    , auth_(auth)
    , rate_limiter_(rate_limiter)
    , config_(config)
{}

void TelegramPoller::start() {
    if (running_) {
        spdlog::warn("Poller already running");
        return;
    }
    
    running_ = true;
    poll_thread_ = std::thread(&TelegramPoller::poll_loop, this);
    spdlog::info("Telegram poller started");
}

void TelegramPoller::stop() {
    if (!running_) return;
    
    running_ = false;
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    spdlog::info("Telegram poller stopped");
}

void TelegramPoller::poll_loop() {
    while (running_) {
        try {
            auto response = tg_client_.get_updates(last_update_id_ + 1, 30);
            
            if (!response["ok"].get<bool>()) {
                spdlog::error("getUpdates failed: {}", response.dump());
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            
            auto& result = response["result"];
            for (const auto& update : result) {
                process_update(update);
                
                int update_id = update["update_id"].get<int>();
                if (update_id >= last_update_id_) {
                    last_update_id_ = update_id;
                }
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Poll loop error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void TelegramPoller::process_update(const nlohmann::json& update) {
    if (!update.contains("message")) return;
    
    const auto& msg = update["message"];
    if (!msg.contains("text") || !msg.contains("from") || !msg.contains("chat")) {
        return;
    }
    
    int64_t chat_id = msg["chat"]["id"].get<int64_t>();
    int64_t user_id = msg["from"]["id"].get<int64_t>();
    std::string text = msg["text"].get<std::string>();
    
    handle_message(chat_id, user_id, text);
}

void TelegramPoller::handle_message(int64_t chat_id, int64_t user_id, const std::string& text) {
    // Rate limiting
    if (!rate_limiter_.check_and_record(user_id)) {
        tg_client_.send_message(chat_id, 
            "⚠️ Rate limit exceeded. Please wait a moment.");
        return;
    }
    
    // Parse command
    auto parsed = CommandParser::parse(text);
    if (!parsed.is_valid()) {
        if (parsed.error) {
            tg_client_.send_message(chat_id, *parsed.error);
        }
        return;
    }
    
    // Special handling for /start (guest pairing)
    if (parsed.cmd == "start") {
        handle_start_command(chat_id, user_id, parsed);
        return;
    }
    
    // Authenticate
    auto auth_result = auth_.authenticate(user_id);
    
    // Check guest session if not owner
    if (auth_result.role == UserRole::Unknown) {
        if (redis_.is_guest_active(user_id)) {
            auth_result.role = UserRole::Guest;
            auth_result.authorized = true;
        }
    }
    
    if (!auth_result.authorized) {
        tg_client_.send_message(chat_id, 
            auth_result.message.value_or("Access denied."));
        return;
    }
    
    // Check command authorization
    if (!auth_.is_command_allowed(parsed.cmd, auth_result.role)) {
        tg_client_.send_message(chat_id, 
            "⛔ You don't have permission for this command.");
        return;
    }
    
    // Handle built-in commands
    if (parsed.cmd == "help") {
        send_help(chat_id, auth_result.role);
        return;
    }
    
    if (parsed.cmd == "guest") {
        handle_guest_command(chat_id, user_id);
        return;
    }
    
    // Publish to Redis for other services
    std::string corr_id = util::generate_uuid();
    auto request_json = CommandParser::to_request_json(
        parsed, user_id, auth_result.role_string(), corr_id);
    
    redis_.publish_request(config_.stream_req, request_json);
    
    spdlog::info("Published command {} from user {} (corr_id: {})", 
                 parsed.cmd, user_id, corr_id);
    
    // For now, send immediate ack (reply consumer will send actual response)
    tg_client_.send_message(chat_id, "⏳ Processing...");
}

void TelegramPoller::handle_start_command(int64_t chat_id, int64_t user_id, 
                                          const ParsedCommand& cmd) {
    auto auth_result = auth_.authenticate(user_id);
    
    if (auth_result.role == UserRole::Owner) {
        tg_client_.send_message(chat_id, 
            "👋 Welcome! You have full access.\n\nSend /help for commands.");
        return;
    }
    
    // Check for guest PIN
    if (!cmd.args.empty()) {
        std::string pin = cmd.args[0];
        auto guest_id = redis_.verify_guest_pin(pin);
        
        if (guest_id) {
            // Set guest session
            redis_.set_guest_session(user_id, config_.guest_default_minutes * 60);
            
            // Audit
            nlohmann::json audit = {
                {"event", "guest_login"},
                {"actor", {{"tg_user_id", user_id}, {"role", "guest"}}},
                {"detail", "PIN authenticated"},
                {"ts", util::current_iso8601()}
            };
            redis_.publish_audit(config_.stream_audit, audit);
            
            tg_client_.send_message(chat_id, 
                "✅ Guest access granted!\n\nYou have read-only access.\nSend /help for commands.");
            return;
        } else {
            tg_client_.send_message(chat_id, "❌ Invalid or expired PIN.");
            return;
        }
    }
    
    tg_client_.send_message(chat_id, 
        "🔒 This bot is private.\n\nIf you have a guest PIN, send: /start <PIN>");
}

void TelegramPoller::handle_guest_command(int64_t chat_id, int64_t user_id) {
    // Generate PIN
    std::string pin = util::generate_pin(6);
    int ttl_seconds = config_.guest_default_minutes * 60;
    
    redis_.store_guest_pin(pin, user_id, ttl_seconds);
    
    tg_client_.send_message(chat_id, 
        fmt::format("🔑 Guest PIN: <code>{}</code>\n\n"
                    "Valid for {} minutes.\n"
                    "Guest sends: /start {}", 
                    pin, config_.guest_default_minutes, pin));
}

void TelegramPoller::send_help(int64_t chat_id, UserRole role) {
    std::string help_text = "<b>SoulScout Commands</b>\n\n";
    
    help_text += "<b>Portfolio:</b>\n";
    help_text += "/balance - View portfolio balance\n";
    help_text += "/holdings - View top holdings\n\n";
    
    help_text += "<b>Signals:</b>\n";
    help_text += "/signals [24h] - View trading signals\n\n";
    
    if (role == UserRole::Owner) {
        help_text += "<b>Control (Owner Only):</b>\n";
        help_text += "/silence [minutes] - Mute alerts\n";
        help_text += "/resume - Resume alerts\n";
        help_text += "/add_wallet <address> - Track wallet\n";
        help_text += "/remove_wallet <address> - Untrack wallet\n";
        help_text += "/guest [minutes] - Generate guest PIN\n\n";
    }
    
    help_text += "<b>System:</b>\n";
    help_text += "/health - System status\n";
    help_text += "/help - Show this help";
    
    tg_client_.send_message(chat_id, help_text);
}