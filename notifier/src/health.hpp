#pragma once
#include "redis_bus.hpp"
#include <nlohmann/json.hpp>
#include <memory>

class HealthCheck {
public:
    HealthCheck(std::shared_ptr<RedisBus> redis, bool muted);
    
    nlohmann::json get_status();
    void set_muted(bool muted);
    bool is_healthy() const;
    
private:
    std::shared_ptr<RedisBus> redis_;
    bool muted_;
};