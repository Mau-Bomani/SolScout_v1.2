#include "config.hpp"
#include "redis_bus.hpp"
#include "pg_store.hpp"
#include "state.hpp"
#include "signals.hpp"
#include "scoring.hpp"
#include "entry_exit.hpp"
#include "throttles.hpp"
#include "regime.hpp"
#include "api_signals.hpp"
#include "health.hpp"
#include "util.hpp"
#include <httplib.h>
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
    spdlog::info("Logging initialized at level: {}", log_level);
}

int main() {
    try {
        // Load configuration
        Config config = Config::load_from_env();
        setup_logging(config.log_level);
        
        spdlog::info("Starting {} on {}:{}", 
                     config.service_name, config.listen_addr, config.listen_port);
        
        // Initialize components
        RedisBus redis(config.redis_url);
        PgStore pgstore(config.pg_dsn);
        MarketState state;
        Throttles throttles(config, redis);
        RegimeFilter regime;
        HealthMonitor health;
        
        // Test connections
        if (!redis.ping()) {
            spdlog::error("Failed to connect to Redis");
            return 1;
        }
        if (!pgstore.ping()) {
            spdlog::error("Failed to connect to Postgres");
            return 1;
        }
        
        health.set_redis(true);
        health.set_postgres(true);
        health.set_loop_status("idle");
        
        // Setup HTTP server for /health endpoint
        httplib::Server http_server;
        
        http_server.Get("/health", [&health](const httplib::Request&, httplib::Response& res) {
            auto health_json = health.to_json();
            res.set_content(health_json, "application/json");
            res.status = health.is_ok() ? 200 : 503;
        });
        
        // Start HTTP server in background thread
        std::thread http_thread([&]() {
            spdlog::info("HTTP server listening on {}:{}", 
                         config.listen_addr, config.listen_port);
            http_server.listen(config.listen_addr.c_str(), config.listen_port);
        });
        
        // Register signal handlers
        signal(SIGTERM, signal_handler);
        signal(SIGINT, signal_handler);
        
        // Main processing loop
        spdlog::info("Entering main loop");
        health.set_loop_status("running");
        
        while (!shutdown_requested) {
            try {
                // 1. Consume market updates
                auto market_updates = redis.consume_stream(
                    config.stream_market, 
                    "analytics_group", 
                    "analytics_consumer",
                    100  // timeout ms
                );
                
                for (const auto& update : market_updates) {
                    try {
                        // Update market state
                        state.update(update);
                        
                        // Extract token/pool identifiers
                        std::string symbol = update["symbol"].get<std::string>();
                        std::string pool = update["pool"].get<std::string>();
                        
                        // Compute signals S1-S10, N1
                        SignalResult signals = compute_signals(update, state, pgstore);
                        
                        // Apply DQ gate
                        if (signals.data_quality < 0.7) {
                            spdlog::debug("DQ gate: {} below 0.7, forcing heads_up", 
                                          signals.data_quality);
                            signals.force_heads_up = true;
                        }
                        
                        // Compute confidence score with penalties
                        double confidence = compute_confidence(signals, update, state);
                        
                        // Apply risk regime filter (adjusts thresholds, not C)
                        auto regime_adj = regime.assess(state);
                        int threshold = config.actionable_base_threshold + regime_adj.threshold_adj;
                        
                        // Apply entry confirmations and net-edge check
                        EntryContext entry_ctx = check_entry_conditions(update, state, signals);
                        
                        if (!entry_ctx.confirmed) {
                            spdlog::debug("Entry not confirmed for {}, blocking actionable", symbol);
                        }
                        
                        double net_edge = calculate_net_edge(update, state);
                        if (net_edge < 0) {
                            spdlog::debug("Net edge negative for {}, forcing heads_up", symbol);
                            entry_ctx.confirmed = false;
                        }
                        
                        // Determine alert band
                        std::string band = "none";
                        if (signals.force_heads_up || !entry_ctx.confirmed) {
                            if (confidence >= 60) {
                                band = "heads_up";
                            }
                        } else if (confidence >= 85 && !signals.has_risk_flags) {
                            band = "high_conviction";
                        } else if (confidence >= threshold) {
                            band = "actionable";
                        } else if (confidence >= 60) {
                            band = "heads_up";
                        }
                        
                        // Apply throttles and cooldowns
                        if (band != "none") {
                            auto throttle_result = throttles.check_and_update(
                                symbol, band, confidence, signals.reasons
                            );
                            
                            if (throttle_result.suppressed) {
                                spdlog::debug("Alert suppressed for {}: {}", 
                                              symbol, throttle_result.reason);
                                continue;
                            }
                            
                            // Build and publish alert
                            nlohmann::json alert = build_alert(
                                band, symbol, update, confidence, 
                                signals.reasons, entry_ctx, regime_adj, state, pgstore
                            );
                            
                            redis.publish_to_stream(config.stream_alerts, alert);
                            health.update_last_decision();
                            
                            spdlog::info("Published {} alert for {} (C={})", 
                                         band, symbol, static_cast<int>(confidence));
                        }
                        
                    } catch (const std::exception& e) {
                        spdlog::error("Error processing market update: {}", e.what());
                    }
                }
                
                // 2. Handle /signals command requests
                auto cmd_requests = redis.consume_stream(
                    config.stream_req,
                    "analytics_cmd_group",
                    "analytics_cmd_consumer",
                    100  // timeout ms
                );
                
                for (const auto& req : cmd_requests) {
                    try {
                        if (req["cmd"].get<std::string>() == "signals") {
                            auto reply = handle_signals_command(req, state, throttles);
                            redis.publish_to_stream(config.stream_rep, reply);
                            spdlog::debug("Replied to /signals command");
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Error handling command: {}", e.what());
                    }
                }
                
                // Update health
                health.set_redis(redis.ping());
                health.set_postgres(pgstore.ping());
                
                // Small sleep to prevent tight loop
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
            } catch (const std::exception& e) {
                spdlog::error("Error in main loop: {}", e.what());
                health.set_loop_status("error");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                health.set_loop_status("running");
            }
        }
        
        // Graceful shutdown
        spdlog::info("Shutting down gracefully");
        health.set_loop_status("shutdown");
        http_server.stop();
        if (http_thread.joinable()) {
            http_thread.join();
        }
        
        spdlog::info("Shutdown complete");
        return 0;
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}