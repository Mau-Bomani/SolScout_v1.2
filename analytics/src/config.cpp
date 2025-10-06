#include "config.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

std::string Config::get_env(const char* name, const std::string& default_val) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : default_val;
}

int Config::get_env_int(const char* name, int default_val) {
    const char* val = std::getenv(name);
    if (!val) return default_val;
    try {
        return std::stoi(val);
    } catch (...) {
        spdlog::warn("Invalid integer for {}, using default {}", name, default_val);
        return default_val;
    }
}

Config Config::from_env() {
    Config cfg;
    
    cfg.redis_url = get_env("REDIS_URL", "redis://localhost:6379");
    cfg.stream_market = get_env("STREAM_MARKET", "soul.market.updates");
    cfg.stream_alerts = get_env("STREAM_ALERTS", "soul.alerts");
    cfg.stream_req = get_env("STREAM_REQ", "soul.cmd.requests");
    cfg.stream_rep = get_env("STREAM_REP", "soul.cmd.replies");
    
    cfg.pg_dsn = get_env("PG_DSN");
    
    // v1.1 thresholds
    cfg.actionable_base_threshold = get_env_int("ACTIONABLE_BASE_THRESHOLD", 70);
    cfg.risk_on_adj = get_env_int("RISK_ON_ADJ", -10);
    cfg.risk_off_adj = get_env_int("RISK_OFF_ADJ", 10);
    cfg.global_actionable_max_per_hour = get_env_int("GLOBAL_ACTIONABLE_MAX_PER_HOUR", 5);
    
    cfg.cooldown_actionable_hours = get_env_int("COOLDOWN_ACTIONABLE_HOURS", 6);
    cfg.cooldown_headsup_hours = get_env_int("COOLDOWN_HEADSUP_HOURS", 1);
    cfg.watch_window_min = get_env_int("WATCH_WINDOW_MIN", 120);
    cfg.reentry_guard_hours = get_env_int("REENTRY_GUARD_HOURS", 12);
    
    cfg.listen_addr = get_env("LISTEN_ADDR", "0.0.0.0");
    cfg.listen_port = get_env_int("LISTEN_PORT", 8083);
    
    cfg.service_name = get_env("SERVICE_NAME", "analytics");
    cfg.log_level = get_env("LOG_LEVEL", "info");
    
    return cfg;
}

void Config::validate() const {
    if (pg_dsn.empty()) {
        throw std::runtime_error("PG_DSN is required");
    }
    
    spdlog::info("Configuration validated successfully");
    spdlog::info("  Actionable threshold: {}", actionable_base_threshold);
    spdlog::info("  Risk adjustments: on={}, off={}", risk_on_adj, risk_off_adj);
    spdlog::info("  Cooldowns: actionable={}h, headsup={}h", 
                 cooldown_actionable_hours, cooldown_headsup_hours);
}