#include "config.hpp"
#include "telegram_client.hpp"
#include "redis_bus.hpp"
#include "auth.hpp"
#include "rate_limiter.hpp"
#include "poller.hpp"
#include "webhook_server.hpp"
#include "health.hpp"
#include "util.hpp"
#include "parser.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    spdlog::info("Received signal {}, initiating shutdown", signal);
    shutdown_requested = true;
}

void setup_logging(const std::string& log_level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("soulscout", console_sink);
    
    if (log_level == "debug") {
        logger->set_level(spdlog::level::debug);
    } else if (log_level == "warn") {
        logger->set_level(spdlog::level::warn);
    } else if (log_level == "error") {
        logger->set_level(spdlog::level::err);
    } else {
        logger->set_level(spdlog::level::info);
    }
    
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
}

// Reply consumer thread
void reply_consumer_loop(TelegramClient& tg_client,
                         RedisBus& redis,
                         const Config& config,
                         std::atomic<bool>& running) {
    
    spdlog::info("Starting reply consumer");
    
    // Create consumer group
    redis.create_consumer_group(config.stream_rep, "tg_gateway");
    
    while (running) {
        try {
            auto replies = redis.read_replies(config.stream_rep, "tg_gateway", 
                                              "consumer1", 10, 1000);
            
            for (const auto& [msg_id, reply_json] : replies) {
                try {
                    // Extract data from reply
                    std::string message = reply_json.value("message", "No response");
                    
                    // For now, we need to map corr_id back to chat_id
                    // In production, maintain a corr_id -> chat_id mapping in Redis
                    // For simplicity, we'll log it
                    spdlog::info("Received reply: {}", message);
                    
                    // Ack the message
                    redis.ack_message(config.stream_rep, "tg_gateway", msg_id);
                    
                } catch (const std::exception& e) {
                    spdlog::error("Failed to process reply: {}", e.what());
                }
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Reply consumer error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    spdlog::info("Reply consumer stopped");
}

// Alert consumer thread
void alert_consumer_loop(TelegramClient& tg_client,
                         RedisBus& redis,
                         const Config& config,
                         int64_t owner_id,
                         std::atomic<bool>& running) {
    
    spdlog::info("Starting alert consumer");
    
    // Create consumer group
    redis.create_consumer_group(config.stream_alerts, "tg_gateway");
    
    while (running) {
        try {
            auto alerts = redis.read_alerts(config.stream_alerts, "tg_gateway",
                                           "consumer1", 10, 1000);
            
            for (const auto& [msg_id, alert_json] : alerts) {
                try {
                    // Extract alert data
                    std::string text = alert_json.value("text", "");
                    std::string to = alert_json.value("to", "owner");
                    
                    if (to == "owner" && !text.empty()) {
                        tg_client.send_message(owner_id, text);
                        spdlog::info("Forwarded alert to owner");
                    }
                    
                    // Ack the message
                    redis.ack_message(config.stream_alerts, "tg_gateway", msg_id);
                    
                } catch (const std::exception& e) {
                    spdlog::error("Failed to process alert: {}", e.what());
                }
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Alert consumer error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    spdlog::info("Alert consumer stopped");
}

int main(int argc, char* argv[]) {
    try {
        // Load configuration
        auto config = Config::from_env();
        
        // Setup logging
        setup_logging(config.log_level);
        
        spdlog::info("==============================================");
        spdlog::info("SoulScout Telegram Gateway v1.0");
        spdlog::info("==============================================");
        
        // Validate config
        config.validate();
        
        // Setup signal handling
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Initialize components
        TelegramClient tg_client(config.tg_bot_token);
        RedisBus redis(config.redis_url);
        Authenticator auth(config.owner_telegram_id);
        RateLimiter rate_limiter(config.rate_limit_msgs_per_min);
        HealthCheck health(redis, config);
        
        // Start reply and alert consumers
        std::atomic<bool> consumers_running{true};
        
        std::thread reply_thread(reply_consumer_loop, 
                                std::ref(tg_client),
                                std::ref(redis),
                                std::ref(config),
                                std::ref(consumers_running));
        
        std::thread alert_thread(alert_consumer_loop,
                                std::ref(tg_client),
                                std::ref(redis),
                                std::ref(config),
                                config.owner_telegram_id,
                                std::ref(consumers_running));
        
        // Start gateway based on mode
        if (config.gateway_mode == "poll") {
            spdlog::info("Starting in POLL mode");
            
            // Delete any existing webhook
            tg_client.delete_webhook();
            
            TelegramPoller poller(tg_client, redis, auth, rate_limiter, config);
            poller.start();
            
            // Start health endpoint on separate server
            WebhookServer health_server(config, tg_client, redis, auth, rate_limiter);
            health_server.start();
            
            // Main loop
            while (!shutdown_requested) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                // Periodic cleanup
                static int cleanup_counter = 0;
                if (++cleanup_counter >= 300) { // Every 5 minutes
                    rate_limiter.cleanup_old_entries();
                    cleanup_counter = 0;
                }
            }
            
            poller.stop();
            health_server.stop();
            
        } else if (config.gateway_mode == "webhook") {
            spdlog::info("Starting in WEBHOOK mode");
            
            // Set webhook
            if (!tg_client.set_webhook(config.webhook_public_url)) {
                throw std::runtime_error("Failed to set webhook");
            }
            spdlog::info("Webhook set to: {}", config.webhook_public_url);
            
            WebhookServer server(config, tg_client, redis, auth, rate_limiter);
            server.start();
            
            // Main loop
            while (!shutdown_requested) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            server.stop();
        }
        
        // Stop consumers
        spdlog::info("Stopping consumers...");
        consumers_running = false;
        
        if (reply_thread.joinable()) reply_thread.join();
        if (alert_thread.joinable()) alert_thread.join();
        
        spdlog::info("Shutdown complete");
        return 0;
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    } 
              