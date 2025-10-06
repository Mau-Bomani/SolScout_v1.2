#include "redis_bus.hpp"
#include <spdlog/spdlog.h>

RedisBus::RedisBus(const std::string& redis_url) {
    redis_ = std::make_shared<sw::redis::Redis>(redis_url);
    spdlog::info("Connected to Redis");
}

void RedisBus::create_consumer_group(const std::string& stream, const std::string& group) {
    try {
        redis_->xgroup_create(stream, group, "$", true);
    } catch (const sw::redis::Error&) {}
}

std::vector<std::pair<std::string, nlohmann::json>>
RedisBus::read_alerts(const std::string& stream, const std::string& group,
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
        spdlog::error("Failed to read alerts: {}", e.what());
    }
    return results;
}

std::vector<std::pair<std::string, nlohmann::json>>
RedisBus::read_commands(const std::string& stream, const std::string& group,
                       const std::string& consumer, int count, int block_ms) {
    return read_alerts(stream, group, consumer, count, block_ms);
}

void RedisBus::ack_message(const std::string& stream, const std::string& group,
                           const std::string& msg_id) {
    try {
        redis_->xack(stream, group, msg_id);
    } catch (const std::exception& e) {
        spdlog::error("Failed to ack: {}", e.what());
    }
}

void RedisBus::publish_outbound_alert(const std::string& stream, const nlohmann::json& data) {
    try {
        std::unordered_map<std::string, std::string> fields;
        fields["data"] = data.dump();
        redis_->xadd(stream, "*", fields.begin(), fields.end());
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish: {}", e.what());
    }
}

void RedisBus::publish_reply(const std::string& stream, const nlohmann::json& data) {
    publish_outbound_alert(stream, data);
}

bool RedisBus::ping() {
    try { redis_->ping(); return true; } catch (...) { return false; }
}