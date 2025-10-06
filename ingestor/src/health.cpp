#include "health.hpp"

HealthCheck::HealthCheck(std::shared_ptr<RedisBus> redis,
                         std::shared_ptr<PostgresStore> pg)
    : redis_(redis), pg_(pg), rpc_status_("up") {}

void HealthCheck::update_dex_status(const std::string& dex, const std::string& status) {
    dex_status_[dex] = status;
}

void HealthCheck::set_rpc_status(const std::string& status) {
    rpc_status_ = status;
}

nlohmann::json HealthCheck::get_status() {
    bool redis_ok = redis_->ping();
    bool pg_ok = pg_->ping();
    
    nlohmann::json dex_json;
    for (const auto& [dex, status] : dex_status_) {
        dex_json[dex] = status;
    }
    
    nlohmann::json status = {
        {"ok", redis_ok && pg_ok},
        {"redis", redis_ok},
        {"postgres", pg_ok},
        {"rpc", rpc_status_},
        {"dex", dex_json},
        {"jupiter", "up"}
    };
    
    return status;
}

bool HealthCheck::is_healthy() const {
    return redis_->ping() && pg_->ping();
}