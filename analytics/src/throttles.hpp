#pragma once

#include <string>
#include <map>
#include <deque>
#include <mutex>

struct AlertRecord {
    std::string symbol;
    std::string band;
    std::string reason_hash;
    int64_t timestamp_ms;
};

class ThrottleManager {
public:
    // Per-token cooldowns
    bool check_token_cooldown(const std::string& symbol, const std::string& band,
                             int cooldown_hours);
    void record_alert(const std::string& symbol, const std::string& band,
                     const std::string& reason_hash);
    
    // Global throttle
    bool check_global_limit(int max_per_hour);
    void record_global_alert();
    
    // Deduplication by reason hash
    bool is_duplicate(const std::string& symbol, const std::string& reason_hash,
                     int ttl_hours);
    
    // Re-entry guard
    bool check_reentry_guard(const std::string& symbol, int guard_hours);
    void record_stop(const std::string& symbol);
    
    // Cleanup
    void cleanup_old_records(int max_age_hours);
    
private:
    std::mutex mutex_;
    std::map<std::string, std::deque<AlertRecord>> token_history_;
    std::deque<int64_t> global_alert_times_;
    std::map<std::string, int64_t> stop_times_;
    
    std::string make_key(const std::string& symbol, const std::string& band);
};