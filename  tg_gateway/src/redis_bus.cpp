#include "redis_bus.hpp"
#include "util.hpp"
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

void RedisBus::publish_request(const std::string& stream, const nlohmann::json& data) {
    try {
        std::unordered_map<std::string, std::string> fields;
        fields["data"] = data.dump();
        
        redis_->xadd(stream, "*", fields.begin(), fields.end());
        spdlog::debug("Published request to {}: {}", stream, data["cmd"].get<std::string>());
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish request: {}", e.what());
        throw;
    }
}

void RedisBus::publish_audit(const std::string& stream, const nlohmann::json& data) {
    try {
        std::unordered_map<std::string, std::string> fields;
        fields["data"] = data.dump();
        
        redis_->xadd(stream, "*", fields.begin(), fields.end());
        spdlog::debug("Published audit event: {}", data["event"].get<std::string>());
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish audit: {}", e.what());
    }
}

void RedisBus::create_consumer_group(const std::string& stream, const std::string& group) {
    try {
        redis_->xgroup_create(stream, group, "$", true);
        spdlog::info("Created consumer group {} on stream {}", group, stream);
    } catch (const sw::redis::Error& e) {
        // Group might already exist, that's ok
        spdlog::debug("Consumer group may already exist: {}", e.what());
    }
}

std::vector<std::pair<std::string, nlohmann::json>>
RedisBus::read_replies(const std::string& stream, const std::string& group,
                       const std::string& consumer, int count, int block_ms) {
    std::vector<std::pair<std::string, nlohmann::json>> results;
    
    try {
        std::unordered_map<std::string, sw::redis::ItemStream> items;
        
        redis_->xreadgroup(group, consumer,
            stream, ">",
            count,
            std::chrono::milliseconds(block_ms),
            std::inserter(items, items.end()));
        
        for (const auto& [stream_name, item_stream] : items) {
            for (const auto& item : item_stream) {
                auto it = item.second.find("data");
                if (it != item.second.end()) {
                    try {
                        auto json_data = nlohmann::json::parse(it->second);
                        results.emplace_back(item.first, json_data);
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to parse message JSON: {}", e.what());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to read replies: {}", e.what());
    }
    
    return results;
}

std::vector<std::pair<std::string, nlohmann::json>>
RedisBus::read_alerts(const std::string& stream, const std::string& group,
                      const std::string& consumer, int count, int block_ms) {
    return read_replies(stream, group, consumer, count, block_ms);
}

void RedisBus::ack_message(const std::string& stream, const std::string& group,
                           const std::string& msg_id) {
    try {
        redis_->xack(stream, group, msg_id);
    } catch (const std::exception& e) {
        spdlog::error("Failed to ack message: {}", e.what());
    }
}

bool RedisBus::store_guest_pin(const std::string& pin, int64_t tg_user_id, int ttl_seconds) {
    try {
        std::string key = guest_pin_key(pin);
        redis_->setex(key, ttl_seconds, std::to_string(tg_user_id));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to store guest PIN: {}", e.what());
        return false;
    }
}

std::optional<int64_t> RedisBus::verify_guest_pin(const std::string& pin) {
    try {
        std::string key = guest_pin_key(pin);
        auto val = redis_->get(key);
        if (val) {
            return std::stoll(*val);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to verify guest PIN: {}", e.what());
    }
    return std::nullopt;
}

void RedisBus::set_guest_session(int64_t tg_user_id, int ttl_seconds) {
    try {
        std::string key = guest_session_key(tg_user_id);
        redis_->setex(key, ttl_seconds, "1");
    } catch (const std::exception& e) {
        spdlog::error("Failed to set guest session: {}", e.what());
    }
}

bool RedisBus::is_guest_active(int64_t tg_user_id) {
    try {
        std::string key = guest_session_key(tg_user_id);
        auto val = redis_->get(key);
        return val.has_value();
    } catch (const std::exception& e) {
        spdlog::error("Failed to check guest session: {}", e.what());
        return false;
    }
}

bool RedisBus::ping() {
    try {
        redis_->ping();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Redis ping failed: {}", e.what());
        return false;
    }
}

std::string RedisBus::guest_pin_key(const std::string& pin) const {
    return "gateway:guest_pin:" + pin;
}

std::string RedisBus::guest_session_key(int64_t tg_user_id) const {
    return "gateway:guest_session:" + std::to_string(tg_user_id);
}