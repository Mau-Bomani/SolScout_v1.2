#include "throttle.hpp"
#include "util.hpp"
#include <algorithm>

GlobalThrottle::GlobalThrottle(int max_per_hour) : max_per_hour_(max_per_hour) {}

bool GlobalThrottle::check_and_record() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now_ms = util::current_timestamp_ms();
    int64_t one_hour_ago = now_ms - (3600 * 1000);
    
    // Remove old entries
    while (!alert_times_ms_.empty() && alert_times_ms_.front() < one_hour_ago) {
        alert_times_ms_.pop_front();
    }
    
    // Check limit
    if (static_cast<int>(alert_times_ms_.size()) >= max_per_hour_) {
        return false;
    }
    
    alert_times_ms_.push_back(now_ms);
    return true;
}

void GlobalThrottle::cleanup_old() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now_ms = util::current_timestamp_ms();
    int64_t one_hour_ago = now_ms - (3600 * 1000);
    
    while (!alert_times_ms_.empty() && alert_times_ms_.front() < one_hour_ago) {
        alert_times_ms_.pop_front();
    }
}

int GlobalThrottle::get_current_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(alert_times_ms_.size());
}