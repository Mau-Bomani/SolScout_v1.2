#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>

class RedisBus {
public:
    explicit RedisBus(const std::string& redis_url);
    
    void publish_market_update(const std::string& stream, const nlohmann::json& data);
    bool ping();
    
private:
    std::shared_ptr<sw::redis::Redis> redis_;
};