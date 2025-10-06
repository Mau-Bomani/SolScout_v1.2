#pragma once

#include <string>
#include <cstdlib>
#include <stdexcept>

struct Config {
    // Telegram
    std::string tg_bot_token;
    int64_t owner_telegram_id;
    
    // Redis
    std::string redis_url;
    std::string stream_req;
    std::string stream_rep;
    std::string stream_alerts;
    std::string stream_audit;
    
    // Gateway mode
    std::string gateway_mode;  // "webhook" or "poll"
    std::string webhook_public_url;
    
    // HTTP server
    std::string listen_addr;
    int listen_port;
    
    // Rate limiting
    int rate_limit_msgs_per_min;
    int global_actionable_max_per_hour;
    int guest_default_minutes;
    
    // Service
    std::string service_name;
    std::string log_level;
    
    static Config from_env();
    void validate() const;
    
private:
    static std::string get_env(const char* name, const std::string& default_val = "");
    static int get_env_int(const char* name, int default_val);
};
