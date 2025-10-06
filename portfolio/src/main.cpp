#include "config.hpp"
#include "redis_bus.hpp"
#include "rpc_solana.hpp"
#include "price_oracle.hpp"
#include "cg_client.hpp"
#include "dex_client.hpp"
#include "jupiter_client.hpp"
#include "metadata_cache.hpp"
#include "valuation.hpp"
#include "postgres_store.hpp"
#include "health.hpp"
#include "util.hpp"
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <signal.h>
#include <atomic>
#include <thread>

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

void handle_balance_command(const nlohmann::json& cmd,
                            SolanaRPC& rpc,
                            PriceOracle& oracle,
                            Valuator& valuator,
                            MetadataCache& metadata,
                            PostgresStore& pg,
                            RedisBus& redis,
                            const Config& config) {
    try {
        int64_t tg_user_id = cmd["from"]["tg_user_id"].get<int64_t>();
        std::string role = cmd["from"]["role"].get<std::string>();
        std::string corr_id = cmd["corr_id"].get<std::string>();
        
        // Get user and wallets
        int64_t user_id = pg.create_or_get_user(tg_user_id, role);
        auto wallets = pg.get_active_wallets(user_id);
        
        if (wallets.empty()) {
            nlohmann::json reply = {
                {"corr_id", corr_id},
                {"ok", false},
                {"message", "No wallets configured. Owner: use /add_wallet <address>"},
                {"ts", util::current_iso8601()}
            };
            redis.publish_reply(config.stream_rep, reply);
            return;
        }
        
        // Fetch and value holdings from all wallets
        std::vector<Holding> all_holdings;
        
        for (const auto& wallet : wallets) {
            auto token_accounts = rpc.get_token_accounts(wallet);
            
            for (const auto& ta : token_accounts) {
                auto meta = metadata.get_or_fetch(ta.mint);
                
                Holding h;
                h.mint = ta.mint;
                h.symbol = meta.symbol;
                h.amount = static_cast<double>(ta.amount) / std::pow(10, ta.decimals);
                
                // Price the holding
                oracle.price_holding(h);
                
                all_holdings.push_back(h);
            }
        }
        
        // Value portfolio
        auto summary = valuator.value_portfolio(all_holdings);
        
        // Save snapshot (use first wallet for now)
        // In production, handle multi-wallet properly
        if (!wallets.empty()) {
            int64_t wallet_id = 1; // Simplified
            int64_t snapshot_id = pg.save_snapshot(wallet_id, summary);
            pg.save_holdings(snapshot_id, summary.holdings);
        }
        
        // Build reply message
        std::string message = fmt::format(
            "💼 Portfolio Balance\n\n"
            "Total: ${:.2f} USD\n"
            "Assets: {} included\n"
            "{}\n"
            "Updated: {}",
            summary.total_usd,
            summary.included_count,
            summary.notes.empty() ? "" : summary.notes,
            util::current_iso8601().substr(0, 19)
        );
        
        nlohmann::json reply = {
            {"corr_id", corr_id},
            {"ok", true},
            {"message", message},
            {"data", {
                {"total_usd", summary.total_usd},
                {"included_count", summary.included_count},
                {"excluded_count", summary.excluded_count}
            }},
            {"ts", util::current_iso8601()}
        };
        
        redis.publish_reply(config.stream_rep, reply);
        spdlog::info("Processed /balance for user {}", tg_user_id);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to handle balance command: {}", e.what());
    }
}

void handle_holdings_command(const nlohmann::json& cmd,
                             SolanaRPC& rpc,
                             PriceOracle& oracle,
                             Valuator& valuator,
                             MetadataCache& metadata,
                             PostgresStore& pg,
                             RedisBus& redis,
                             const Config& config) {
    try {
        int64_t tg_user_id = cmd["from"]["tg_user_id"].get<int64_t>();
        std::string role = cmd["from"]["role"].get<std::string>();
        std::string corr_id = cmd["corr_id"].get<std::string>();
        int limit = cmd["args"].value("limit", 10);
        
        // Get user and wallets
        int64_t user_id = pg.create_or_get_user(tg_user_id, role);
        auto wallets = pg.get_active_wallets(user_id);
        
        if (wallets.empty()) {
            nlohmann::json reply = {
                {"corr_id", corr_id},
                {"ok", false},
                {"message", "No wallets configured."},
                {"ts", util::current_iso8601()}
            };
            redis.publish_reply(config.stream_rep, reply);
            return;
        }
        
        // Fetch and value holdings
        std::vector<Holding> all_holdings;
        
        for (const auto& wallet : wallets) {
            auto token_accounts = rpc.get_token_accounts(wallet);
            
            for (const auto& ta : token_accounts) {
                auto meta = metadata.get_or_fetch(ta.mint);
                
                Holding h;
                h.mint = ta.mint;
                h.symbol = meta.symbol;
                h.amount = static_cast<double>(ta.amount) / std::pow(10, ta.decimals);
                
                oracle.price_holding(h);
                all_holdings.push_back(h);
            }
        }
        
        auto summary = valuator.value_portfolio(all_holdings);
        
        // Build holdings list (top N)
        std::string message = "📊 Top Holdings\n\n";
        int count = 0;
        for (const auto& h : summary.holdings) {
            if (count >= limit) break;
            
            if (h.usd_value.has_value()) {
                message += fmt::format("{}. {} - {:.4f} (${:.2f})\n",
                    count + 1, h.symbol, h.amount, *h.usd_value);
            } else {
                message += fmt::format("{}. {} - {:.4f} (N/A)\n",
                    count + 1, h.symbol, h.amount);
            }
            count++;
        }
        
        if (summary.holdings.size() > static_cast<size_t>(limit)) {
            message += fmt::format("\n+ {} more...", summary.holdings.size() - limit);
        }
        
        nlohmann::json reply = {
            {"corr_id", corr_id},
            {"ok", true},
            {"message", message},
            {"ts", util::current_iso8601()}
        };
        
        redis.publish_reply(config.stream_rep, reply);
        spdlog::info("Processed /holdings for user {}", tg_user_id);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to handle holdings command: {}", e.what());
    }
}

void handle_add_wallet(const nlohmann::json& cmd,
                       PostgresStore& pg,
                       RedisBus& redis,
                       const Config& config) {
    try {
        int64_t tg_user_id = cmd["from"]["tg_user_id"].get<int64_t>();
        std::string role = cmd["from"]["role"].get<std::string>();
        std::string corr_id = cmd["corr_id"].get<std::string>();
        
        if (role != "owner") {
            nlohmann::json reply = {
                {"corr_id", corr_id},
                {"ok", false},
                {"message", "Only owner can add wallets."},
                {"ts", util::current_iso8601()}
            };
            redis.publish_reply(config.stream_rep, reply);
            return;
        }
        
        if (!cmd["args"].contains("address")) {
            nlohmann::json reply = {
                {"corr_id", corr_id},
                {"ok", false},
                {"message", "Usage: /add_wallet <address>"},
                {"ts", util::current_iso8601()}
            };
            redis.publish_reply(config.stream_rep, reply);
            return;
        }
        
        std::string address = cmd["args"]["address"].get<std::string>();
        
        if (!util::is_valid_solana_address(address)) {
            nlohmann::json reply = {
                {"corr_id", corr_id},
                {"ok", false},
                {"message", "Invalid Solana address format."},
                {"ts", util::current_iso8601()}
            };
            redis.publish_reply(config.stream_rep, reply);
            return;
        }
        
        int64_t user_id = pg.create_or_get_user(tg_user_id, role);
        pg.add_wallet(user_id, address);
        
        // Audit
        nlohmann::json audit = {
            {"event", "wallet_added"},
            {"actor", {{"tg_user_id", tg_user_id}, {"role", role}}},
            {"detail", "wallet=" + address},
            {"ts", util::current_iso8601()}
        };
        redis.publish_audit(config.stream_audit, audit);
        
        nlohmann::json reply = {
            {"corr_id", corr_id},
            {"ok", true},
            {"message", "✅ Wallet added: " + address.substr(0, 8) + "..."},
            {"ts", util::current_iso8601()}
        };
        
        redis.publish_reply(config.stream_rep, reply);
        spdlog::info("Added wallet for user {}", tg_user_id);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to handle add_wallet: {}", e.what());
    }
}

void handle_remove_wallet(const nlohmann::json& cmd,
                          PostgresStore& pg,
                          RedisBus& redis,
                          const Config& config) {
    try {
        int64_t tg_user_id = cmd["from"]["tg_user_id"].get<int64_t>();
        std::string role = cmd["from"]["role"].get<std::string>();
        std::string corr_id = cmd["corr_id"].get<std::string>();
        
        if (role != "owner") {
            nlohmann::json reply = {
                {"corr_id", corr_id},
                {"ok", false},
                {"message", "Only owner can remove wallets."},
                {"ts", util::current_iso8601()}
            };
            redis.publish_reply(config.stream_rep, reply);
            return;
        }
        
        if (!cmd["args"].contains("address")) {
            nlohmann::json reply = {
                {"corr_id", corr_id},
                {"ok", false},
                {"message", "Usage: /remove_wallet <address>"},
                {"ts", util::current_iso8601()}
            };
            redis.publish_reply(config.stream_rep, reply);
            return;
        }
        
        std::string address = cmd["args"]["address"].get<std::string>();
        pg.remove_wallet(address);
        
        // Audit
        nlohmann::json audit = {
            {"event", "wallet_removed"},
            {"actor", {{"tg_user_id", tg_user_id}, {"role", role}}},
            {"detail", "wallet=" + address},
            {"ts", util::current_iso8601()}
        };
        redis.publish_audit(config.stream_audit, audit);
        
        nlohmann::json reply = {
            {"corr_id", corr_id},
            {"ok", true},
            {"message", "✅ Wallet removed: " + address.substr(0, 8) + "..."},
            {"ts", util::current_iso8601()}
        };
        
        redis.publish_reply(config.stream_rep, reply);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to handle remove_wallet: {}", e.what());
    }
}

void command_consumer_loop(std::shared_ptr<Config> config,
                           std::shared_ptr<RedisBus> redis,
                           std::shared_ptr<SolanaRPC> rpc,
                           std::shared_ptr<PriceOracle> oracle,
                           std::shared_ptr<Valuator> valuator,
                           std::shared_ptr<MetadataCache> metadata,
                           std::shared_ptr<PostgresStore> pg,
                           std::atomic<bool>& running) {
    
    spdlog::info("Starting command consumer");
    redis->create_consumer_group(config->stream_req, "portfolio");
    
    while (running) {
        try {
            auto commands = redis->read_commands(config->stream_req, "portfolio",
                                                "consumer1", 10, 1000);
            
            for (const auto& [msg_id, cmd_json] : commands) {
                try {
                    std::string cmd = cmd_json.value("cmd", "");
                    
                    if (cmd == "balance") {
                        handle_balance_command(cmd_json, *rpc, *oracle, *valuator,
                                             *metadata, *pg, *redis, *config);
                    } else if (cmd == "holdings") {
                        handle_holdings_command(cmd_json, *rpc, *oracle, *valuator,
                                              *metadata, *pg, *redis, *config);
                    } else if (cmd == "add_wallet") {
                        handle_add_wallet(cmd_json, *pg, *redis, *config);
                    } else if (cmd == "remove_wallet") {
                        handle_remove_wallet(cmd_json, *pg, *redis, *config);
                    }
                    
                    redis->ack_message(config->stream_req, "portfolio", msg_id);
                    
                } catch (const std::exception& e) {
                    spdlog::error("Failed to process command: {}", e.what());
                }
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Command consumer error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    spdlog::info("Command consumer stopped");
}

int main(int argc, char* argv[]) {
    try {
        auto config = std::make_shared<Config>(Config::from_env());
        setup_logging(config->log_level);
        
        spdlog::info("==============================================");
        spdlog::info("SoulScout Portfolio Service v1.0");
        spdlog::info("==============================================");
        
        config->validate();
        
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Initialize components
        auto redis = std::make_shared<RedisBus>(config->redis_url);
        auto pg = std::make_shared<PostgresStore>(config->pg_dsn);
        auto rpc = std::make_shared<SolanaRPC>(config->rpc_urls, config->request_timeout_ms);
        auto cg = std::make_shared<CoinGeckoClient>(config->coingecko_base, config->request_timeout_ms);
        auto dex = std::make_shared<DEXClient>(config->raydium_base, config->orca_base, config->request_timeout_ms);
        auto jupiter = std::make_shared<JupiterClient>(config->jupiter_base, config->request_timeout_ms);
        auto oracle = std::make_shared<PriceOracle>(cg, dex, jupiter);
        auto valuator = std::make_shared<Valuator>(config->dust_min_usd, config->haircut_low_liq_pct);
        auto metadata = std::make_shared<MetadataCache>();
        auto health = std::make_shared<HealthCheck>(redis, pg, rpc);
        
        // Initialize database schema
        pg->init_schema();
        
        // Start command consumer
        std::atomic<bool> consumer_running{true};
        std::thread consumer_thread(command_consumer_loop, config, redis, rpc,
                                   oracle, valuator, metadata, pg,
                                   std::ref(consumer_running));
        
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
        
        spdlog::info("Portfolio service started");
        
        // Main loop
        while (!shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Shutdown
        spdlog::info("Stopping services...");
        consumer_running = false;
        server.stop();
        
        if (consumer_thread.joinable()) consumer_thread.join();
        if (http_thread.joinable()) http_thread.join();
        
        spdlog::info("Shutdown complete");
        return 0;
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}