#include "health.hpp"

HealthCheck::HealthCheck(std::shared_ptr<RedisBus> redis,
                         std::shared_ptr<PostgresStore> pg,
                         std::shared_ptr<SolanaRPC> rpc)
    : redis_(redis), pg_(pg), rpc_(rpc) {}

nlohmann::json HealthCheck::get_status() const {
    bool redis_ok = redis_->ping();
    bool pg_ok = pg_->ping();
    bool rpc_ok = rpc_->is_healthy();
    
    nlohmann::json status = {
        {"ok", redis_ok && pg_ok},
        {"redis", redis_ok},
        {"postgres", pg_ok},
        {"rpc", rpc_ok ? "up" : "degraded"}
    };
    
    return status;
}

bool HealthCheck::is_healthy() const {
    return redis_->ping() && pg_->ping();
}