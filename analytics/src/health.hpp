#pragma once
#include "redis_bus.hpp"
#include "pg_store.hpp"
#include <nlohmann/json.hpp>
#include <memory>

class HealthCheck {
public:
    HealthCheck(std::shared_ptr<RedisBus> redis, std::shared_ptr<PostgresStore> pg);
    nlohmann::json get_status();
    bool is_healthy() const;
    
private:
    std::shared_ptr<RedisBus> redis_;
    std::shared_ptr<PostgresStore> pg_;
};