#include "health.hpp"

HealthCheck::HealthCheck(std::shared_ptr<RedisBus> redis, bool muted)
    : redis_(redis), muted_(muted) {}

void HealthCheck::set_muted(bool muted) {
    muted_ = muted;
}

nlohmann::json HealthCheck::get_status() {
    bool redis_ok = redis_->ping();
    return {
        {"ok", redis_ok},
        {"redis", redis_ok},
        {"mode", "streaming"},
        {"muted", muted_},
        {"queue_lag", 0}
    };
}

bool HealthCheck::is_healthy() const {
    return redis_->ping();
}