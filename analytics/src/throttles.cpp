#include "throttles.hpp"
#include "util.hpp"
#include <algorithm>

std::string ThrottleManager::make_key(const std::string& symbol, const std::string& band) {
    return symbol + ":" + band;
}

bool ThrottleManager::check_token_cooldown(const std::string& symbol, 
                                          const std::string& band,
                                          int cooldown_hours) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = make_key(symbol, band);
    auto& history = token_history_[key];
    
    if (history.empty()) return true; // No cooldown
    
    int64_t cutoff_ms = util::current_timestamp_ms() - (cooldown_hours * 3600 * 1000);
    
    // Check if last alert is within cooldown
    if (!history.empty() && history.back().timestamp_ms > cutoff_ms) {
        return false; // Still in cooldown
    }
    
    return true;
}

void ThrottleManager::record_alert(const std::string& symbol, 
                                   const std::string& band,
                                   const std::string& reason_hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = make_key(symbol, band);
    AlertRecord rec;
    rec.symbol = symbol;
    rec.band = band;
    rec.reason_hash = reason_hash;
    rec.timestamp_ms = util::current_timestamp_ms();
    
    token_history_[key].push_back(rec);
}

bool ThrottleManager::check_global_limit(int max_per_hour) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t cutoff_ms = util::current_timestamp_ms() - (3600 * 1000);
    
    // Remove old entries
    while (!global_alert_times_.empty() && global_alert_times_.front() < cutoff_ms) {
        global_alert_times_.pop_front();
    }
    
    // Check count
    return static_cast<int>(global_alert_times_.size()) < max_per_hour;
}

void ThrottleManager::record_global_alert() {
    std::lock_guard<std::mutex> lock(mutex_);
    global_alert_times_.push_back(util::current_timestamp_ms());
}

bool ThrottleManager::is_duplicate(const std::string& symbol, 
                                  const std::string& reason_hash,
                                  int ttl_hours) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t cutoff_ms = util::current_timestamp_ms() - (ttl_hours * 3600 * 1000);
    
    // Check all bands for this symbol
    for (const auto& [key, history] : token_history_) {
        if (key.find(symbol) == 0) { // Starts with symbol
            for (const auto& rec : history) {
                if (rec.timestamp_ms > cutoff_ms && rec.reason_hash == reason_hash) {
                    return true; // Duplicate
                }
            }
        }
    }
    
    return false;
}

bool ThrottleManager::check_reentry_guard(const std::string& symbol, int guard_hours) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = stop_times_.find(symbol);
    if (it == stop_times_.end()) return true; // No stop recorded
    
    int64_t cutoff_ms = util::current_timestamp_ms() - (guard_hours * 3600 * 1000);
    
    if (it->second > cutoff_ms) {
        return false; // Still in guard period
    }
    
    return true;
}

void ThrottleManager::record_stop(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_times_[symbol] = util::current_timestamp_ms();
}

void ThrottleManager::cleanup_old_records(int max_age_hours) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t cutoff_ms = util::current_timestamp_ms() - (max_age_hours * 3600 * 1000);
    
    // Clean token history
    for (auto it = token_history_.begin(); it != token_history_.end();) {
        auto& history = it->second;
        
        // Remove old alerts
        while (!history.empty() && history.front().timestamp_ms < cutoff_ms) {
            history.pop_front();
        }
        
        if (history.empty()) {
            it = token_history_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clean stop times
    for (auto it = stop_times_.begin(); it != stop_times_.end();) {
        if (it->second < cutoff_ms) {
            it = stop_times_.erase(it);
        } else {
            ++it;
        }
    }
}