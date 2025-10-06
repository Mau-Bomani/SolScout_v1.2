#pragma once

#include "redis_bus.hpp"
#include "config.hpp"
#include <nlohmann/json.hpp>

class HealthCheck {
public:
    HealthCheck(RedisBus& redis, const Config& config)
        : redis_(redis), config_(config) {}
    
    nlohmann::json get_status() const;
    bool is_healthy() const;
    
private:
    RedisBus& redis_;
    const Config& config_;
};