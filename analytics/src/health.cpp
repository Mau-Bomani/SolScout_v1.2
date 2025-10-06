
#include "health.hpp"
#include "util.hpp"

HealthCheck::HealthCheck(std::shared_ptr<RedisBus> redis, std::shared_ptr<PostgresStore> pg)
    : redis_(redis), pg_(pg) {}

nlohmann::json HealthCheck::get_status() {
    bool redis_ok = redis_->ping();
    bool pg_ok = pg_->ping();
    
    return {
        {"ok", redis_ok && pg_ok},
        {"redis", redis_ok},
        {"postgres", pg_ok},
        {"loop", "running"},
        {"last_decision_ts", util::current_iso8601()}
    };
}

bool HealthCheck::is_healthy() const {
    return redis_->ping() && pg_->ping();
}