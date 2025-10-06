#include "health.hpp"

nlohmann::json HealthCheck::get_status() const {
    bool redis_ok = redis_.ping();
    
    nlohmann::json status = {
        {"ok", redis_ok},
        {"redis", redis_ok},
        {"mode", config_.gateway_mode},
        {"service", config_.service_name}
    };
    
    return status;
}

bool HealthCheck::is_healthy() const {
    return redis_.ping();
}