#pragma once

#include "redis_bus.hpp"
#include "store_pg.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <map>

class HealthCheck {
public:
    HealthCheck(std::shared_ptr<RedisBus> redis,
                std::shared_ptr<PostgresStore> pg);
    
    nlohmann::json get_status();
    bool is_healthy() const;
    
    void update_dex_status(const std::string& dex, const std::string& status);
    void set_rpc_status(const std::string& status);
    
private:
    std::shared_ptr<RedisBus> redis_;
    std::shared_ptr<PostgresStore> pg_;
    std::map<std::string, std::string> dex_status_;
    std::string rpc_status_;
};