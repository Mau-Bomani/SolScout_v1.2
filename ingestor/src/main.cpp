#include "config.hpp"
#include "http_client.hpp"
#include "rpc_clients/raydium_client.hpp"
#include "rpc_clients/orca_client.hpp"
#include "rpc_clients/jupiter_client.hpp"
#include "rpc_clients/solana_rpc_client.hpp"
#include "bar_synth.hpp"
#include "normalize.hpp"
#include "store_pg.hpp"
#include "redis_bus.hpp"
#include "health.hpp"
#include "util.hpp"
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <signal.h>
#include <atomic>
#include <thread>
#include <map>

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

void ingest_loop(std::shared_ptr<Config> config,
                 std::shared_ptr<HttpClient> http,
                 std::shared_ptr<RaydiumClient> raydium,
                 std::shared_ptr<OrcaClient> orca,
                 std::shared_ptr<JupiterClient> jupiter,
                 std::shared_ptr<PostgresStore> pg,
                 std::shared_ptr<RedisBus> redis,
                 std::shared_ptr<HealthCheck> health,
                 std::atomic<bool>& running) {
    
    spdlog::info("Starting ingest loop");
    
    // Bar synthesizers per pool
    std::map<int64_t, std::shared_ptr<BarSynthesizer>> bar_5m;
    std::map<int64_t, std::shared_ptr<BarSynthesizer>> bar_15m;
    
    while (running) {
        auto tick_start = util::current_timestamp_ms();
        
        try {
            // Fetch pools from Raydium
            auto raydium_pools = raydium->get_pools();
            spdlog::debug("Fetched {} Raydium pools", raydium_pools.size());
            health->update_dex_status("raydium", raydium_pools.empty() ? "degraded" : "up");
            
            // Fetch pools from Orca
            auto orca_pools = orca->get_pools();
            spdlog::debug("Fetched {} Orca pools", orca_pools.size());
            health->update_dex_status("orca", orca_pools.empty() ? "degraded" : "up");
            
            // Process each pool
            int processed = 0;
            for (const auto& pool_data : raydium_pools) {
                try {
                    // Create normalized pool structure
                    nlohmann::json raw_json = {
                        {"address", pool_data.address},
                        {"mint_base", pool_data.mint_base},
                        {"mint_quote", pool_data.mint_quote},
                        {"price", pool_data.price},
                        {"liquidity_usd", pool_data.liq_usd},
                        {"volume_24h_usd", pool_data.vol24h_usd}
                    };
                    
                    auto normalized = Normalizer::normalize_pool(raw_json, "raydium");
                    int64_t pool_id = pg->upsert_pool(normalized);
                    
                    // Create synthesizers if needed
                    if (bar_5m.find(pool_id) == bar_5m.end()) {
                        bar_5m[pool_id] = std::make_shared<BarSynthesizer>(config->bar_interval_5m);
                        bar_15m[pool_id] = std::make_shared<BarSynthesizer>(config->bar_interval_15m);
                    }
                    
                    // Add tick to synthesizers
                    PriceTick tick;
                    tick.price = normalized.price;
                    tick.volume_usd = normalized.vol24h_usd / 288.0; // Approximate per-5min volume
                    tick.timestamp_ms = util::current_timestamp_ms();
                    
                    bar_5m[pool_id]->add_tick(tick);
                    bar_15m[pool_id]->add_tick(tick);
                    
                    // Get completed bars and save
                    auto completed_5m = bar_5m[pool_id]->get_completed_bars();
                    for (const auto& bar : completed_5m) {
                        // Get Jupiter route info
                        auto route = jupiter->get_route(normalized.mint_base, "USDC");
                        
                        pg->save_5m_stats(pool_id, bar,
                            normalized.liq_usd, normalized.vol24h_usd,
                            normalized.spread_pct, normalized.impact_1pct_pct,
                            route.ok, route.hops, route.dev_pct,
                            normalized.dq);
                    }
                    
                    auto completed_15m = bar_15m[pool_id]->get_completed_bars();
                    for (const auto& bar : completed_15m) {
                        pg->save_15m_bar(pool_id, bar);
                    }
                    
                    // Track first liquidity
                    pg->update_token_first_liq(normalized.mint_base, normalized.liq_usd, pool_id);
                    
                    // Publish to Redis for Analytics
                    nlohmann::json market_update = {
                        {"pool", normalized.address},
                        {"mint_base", normalized.mint_base},
                        {"mint_quote", normalized.mint_quote},
                        {"price", normalized.price},
                        {"liq_usd", normalized.liq_usd},
                        {"vol24h_usd", normalized.vol24h_usd},
                        {"spread_pct", normalized.spread_pct},
                        {"impact_1pct_pct", normalized.impact_1pct_pct},
                        {"age_hours", 0.0}, // Would calculate from first_liq_ts
                        {"route", {
                            {"ok", true},
                            {"hops", 2},
                            {"dev_pct", 0.3}
                        }},
                        {"bars", {
                            {"5m", {
                                {"o", 0.0}, {"h", 0.0}, {"l", 0.0}, {"c", normalized.price}, {"v_usd", 0.0}
                            }},
                            {"15m", {
                                {"o", 0.0}, {"h", 0.0}, {"l", 0.0}, {"c", normalized.price}, {"v_usd", 0.0}
                            }}
                        }},
                        {"dq", normalized.dq},
                        {"ts", util::current_iso8601()}
                    };
                    
                    redis->publish_market_update(config->stream_market, market_update);
                    processed++;
                    
                } catch (const std::exception& e) {
                    spdlog::error("Failed to process pool: {}", e.what());
                }
            }
            
            spdlog::info("Tick complete: processed {} pools", processed);
            
        } catch (const std::exception& e) {
            spdlog::error("Ingest loop error: {}", e.what());
        }
        
        // Sleep until next tick
        auto tick_duration = util::current_timestamp_ms() - tick_start;
        auto sleep_ms = (config->global_tick_seconds * 1000) - tick_duration;
        
        if (sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }
    }
    
    spdlog::info("Ingest loop stopped");
}

int main(int argc, char* argv[]) {
    try {
        auto config = std::make_shared<Config>(Config::from_env());
        setup_logging(config->log_level);
        
        spdlog::info("==============================================");
        spdlog::info("SoulScout Market Ingestor v1.0");
        spdlog::info("==============================================");
        
        config->validate();
        
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Initialize components
        auto http = std::make_shared<HttpClient>(config->request_timeout_ms);
        http->set_retry_backoff(config->retry_backoff_ms_min, config->retry_backoff_ms_max);
        
        auto redis = std::make_shared<RedisBus>(config->redis_url);
        auto pg = std::make_shared<PostgresStore>(config->pg_dsn);
        auto rpc = std::make_shared<SolanaRPCClient>(config->rpc_urls, http);
        auto raydium = std::make_shared<RaydiumClient>(config->raydium_base, http);
        auto orca = std::make_shared<OrcaClient>(config->orca_base, http);
        auto jupiter = std::make_shared<JupiterClient>(config->jupiter_base, http);
        auto health = std::make_shared<HealthCheck>(redis, pg);
        
        // Initialize database
        pg->init_schema();
        
        // Start ingest loop
        std::atomic<bool> loop_running{true};
        std::thread ingest_thread(ingest_loop, config, http, raydium, orca, jupiter,
                                 pg, redis, health, std::ref(loop_running));
        
        // Start HTTP health server
        httplib::Server server;
        
        server.Get("/health", [health](const httplib::Request&, httplib::Response& res) {
            auto status = health->get_status();
            res.set_content(status.dump(), "application/json");
            res.status = health->is_healthy() ? 200 : 503;
        });
        
        std::thread http_thread([&server, config]() {
            spdlog::info("Starting HTTP server on {}:{}", config->listen_addr, config->listen_port);
            server.listen(config->listen_addr.c_str(), config->listen_port);
        });
        
        spdlog::info("Ingestor started");
        
        // Main loop
        while (!shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Shutdown
        spdlog::info("Stopping services...");
        loop_running = false;
        server.stop();
        
        if (ingest_thread.joinable()) ingest_thread.join();
        if (http_thread.joinable()) http_thread.join();
        
        spdlog::info("Shutdown complete");
        return 0;
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}