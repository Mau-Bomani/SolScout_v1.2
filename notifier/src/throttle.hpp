#pragma once
#include <deque>
#include <mutex>

class GlobalThrottle {
public:
    explicit GlobalThrottle(int max_per_hour);
    
    bool check_and_record();
    void cleanup_old();
    int get_current_count() const;
    
private:
    int max_per_hour_;
    mutable std::mutex mutex_;
    std::deque<int64_t> alert_times_ms_;
}; 