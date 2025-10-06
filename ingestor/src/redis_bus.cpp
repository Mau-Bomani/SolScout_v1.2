#include "redis_bus.hpp"
#include <spdlog/spdlog.h>

RedisBus::RedisBus(const std::string& redis_url) {
    try {
        redis_ = std::make_shared<sw::redis::Redis>(redis_url);
        spdlog::info("Connected to Redis: {}", redis_url);
    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to Redis: {}", e.what());
        throw;
    }
}

void RedisBus::publish_market_update(const std::string& stream, const nlohmann::json& data) {
    try {
        std::unordered_map<std::string, std::string> fields;
        fields["data"] = data.dump();
        
        redis_->xadd(stream, "*", fields.begin(), fields.end());
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish market update: {}", e.what());
    }
}

bool RedisBus::ping() {
    try {
        redis_->ping();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}