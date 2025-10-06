#pragma once

#include "redis_bus.hpp"
#include "postgres_store.hpp"
#include "rpc_solana.hpp"
#include <nlohmann/json.hpp>
#include <memory>

class HealthCheck {
public:
    HealthCheck(std::shared_ptr<RedisBus> redis,
                std::shared_ptr<PostgresStore> pg,
                std::shared_ptr<SolanaRPC> rpc);
    
    nlohmann::json get_status() const;
    bool is_healthy() const;
    
private:
    std::shared_ptr<RedisBus> redis_;
    std::shared_ptr<PostgresStore> pg_;
    std::shared_ptr<SolanaRPC> rpc_;
};
