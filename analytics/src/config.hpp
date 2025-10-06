#pragma once

#include <string>
#include <cstdlib>

struct Config {
    // Redis
    std::string redis_url;
    std::string stream_market;
    std::string stream_alerts;
    std::string stream_req;
    std::string stream_rep;
    
    // Postgres
    std::string pg_dsn;
    
    // Analytics thresholds (v1.1 spec)
    int actionable_base_threshold;
    int risk_on_adj;
    int risk_off_adj;
    int global_actionable_max_per_hour;
    
    // Cooldowns (hours)
    int cooldown_actionable_hours;
    int cooldown_headsup_hours;
    int watch_window_min;
    int reentry_guard_hours;
    
    // HTTP
    std::string listen_addr;
    int listen_port;
    
    // Service
    std::string service_name;
    std::string log_level;
    
    static Config from_env();
    void validate() const;
    
private:
    static std::string get_env(const char* name, const std::string& default_val = "");
    static int get_env_int(const char* name, int default_val);
};