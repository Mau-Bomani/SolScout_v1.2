#include "redis_bus.hpp"
#include <spdlog/spdlog.h>

RedisBus::RedisBus(const std::string& redis_url) {
    redis_ = std::make_shared<sw::redis::Redis>(redis_url);
    spdlog::info("Connected to Redis: {}", redis_url);
}

void RedisBus::create_consumer_group(const std::string& stream, const std::string& group) {
    try {
        redis_->xgroup_create(stream, group, "$", true);
        spdlog::info("Created consumer group {} on {}", group, stream);
    } catch (const sw::redis::Error& e) {
        spdlog::debug("Consumer group may exist: {}", e.what());
    }
}

std::vector<std::pair<std::string, nlohmann::json>>
RedisBus::read_market_updates(const std::string& stream, const std::string& group,
                              const std::string& consumer, int count, int block_ms) {
    std::vector<std::pair<std::string, nlohmann::json>> results;
    
    try {
        std::unordered_map<std::string, sw::redis::ItemStream> items;
        redis_->xreadgroup(group, consumer, stream, ">", count,
                          std::chrono::milliseconds(block_ms),
                          std::inserter(items, items.end()));
        
        for (const auto& [_, item_stream] : items) {
            for (const auto& item : item_stream) {
                auto it = item.second.find("data");
                if (it != item.second.end()) {
                    results.emplace_back(item.first, nlohmann::json::parse(it->second));
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to read market updates: {}", e.what());
    }
    
    return results;
}

void RedisBus::ack_message(const std::string& stream, const std::string& group,
                           const std::string& msg_id) {
    try {
        redis_->xack(stream, group, msg_id);
    } catch (const std::exception& e) {
        spdlog::error("Failed to ack: {}", e.what());
    }
}

void RedisBus::publish_alert(const std::string& stream, const nlohmann::json& data) {
    try {
        std::unordered_map<std::string, std::string> fields;
        fields["data"] = data.dump();
        redis_->xadd(stream, "*", fields.begin(), fields.end());
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish alert: {}", e.what());
    }
}

bool RedisBus::ping() {
    try {
        redis_->ping();
        return true;
    } catch (...) {
        return false;
    }
}
