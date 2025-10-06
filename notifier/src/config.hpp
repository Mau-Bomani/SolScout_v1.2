#pragma once
#include <string>
#include <cstdlib>

struct Config {
    std::string redis_url;
    std::string stream_alerts_in;
    std::string stream_alerts_out;
    std::string stream_req;
    std::string stream_rep;
    std::string pg_dsn;
    
    int global_actionable_max_per_hour;
    int dedup_ttl_seconds;
    int mute_default_minutes;
    int64_t owner_telegram_id;
    std::string user_tz;
    
    std::string listen_addr;
    int listen_port;
    std::string service_name;
    std::string log_level;
    
    static Config from_env();
    void validate() const;
};

#include "config.hpp"
#include <spdlog/spdlog.h>

Config Config::from_env() {
    Config cfg;
    
    auto get_env = [](const char* name, const std::string& def) {
        const char* val = std::getenv(name);
        return val ? std::string(val) : def;
    };
    
    auto get_env_int = [](const char* name, int def) {
        const char* val = std::getenv(name);
        return val ? std::stoi(val) : def;
    };
    
    cfg.redis_url = get_env("REDIS_URL", "redis://localhost:6379");
    cfg.stream_alerts_in = get_env("STREAM_ALERTS_IN", "soul.alerts");
    cfg.stream_alerts_out = get_env("STREAM_ALERTS_OUT", "soul.outbound.alerts");
    cfg.stream_req = get_env("STREAM_REQ", "soul.cmd.requests");
    cfg.stream_rep = get_env("STREAM_REP", "soul.cmd.replies");
    cfg.pg_dsn = get_env("PG_DSN", "");
    
    cfg.global_actionable_max_per_hour = get_env_int("GLOBAL_ACTIONABLE_MAX_PER_HOUR", 5);
    cfg.dedup_ttl_seconds = get_env_int("DEDUP_TTL_SECONDS", 21600);
    cfg.mute_default_minutes = get_env_int("MUTE_DEFAULT_MINUTES", 30);
    cfg.owner_telegram_id = get_env_int("OWNER_TELEGRAM_ID", 0);
    cfg.user_tz = get_env("USER_TZ", "America/Denver");
    
    cfg.listen_addr = get_env("LISTEN_ADDR", "0.0.0.0");
    cfg.listen_port = get_env_int("LISTEN_PORT", 8084);
    cfg.service_name = get_env("SERVICE_NAME", "notifier");
    cfg.log_level = get_env("LOG_LEVEL", "info");
    
    return cfg;
}

void Config::validate() const {
    if (owner_telegram_id == 0) {
        throw std::runtime_error("OWNER_TELEGRAM_ID required");
    }
    spdlog::info("Configuration validated");
}