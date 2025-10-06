#pragma once

#include <string>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>

class RedisBus {
public:
    explicit RedisBus(const std::string& redis_url);
    
    // Stream operations
    void publish_request(const std::string& stream, const nlohmann::json& data);
    void publish_audit(const std::string& stream, const nlohmann::json& data);
    
    // Consumer group operations
    void create_consumer_group(const std::string& stream, const std::string& group);
    std::vector<std::pair<std::string, nlohmann::json>> 
        read_replies(const std::string& stream, const std::string& group, 
                     const std::string& consumer, int count = 10, int block_ms = 1000);
    
    std::vector<std::pair<std::string, nlohmann::json>>
        read_alerts(const std::string& stream, const std::string& group,
                    const std::string& consumer, int count = 10, int block_ms = 1000);
    
    void ack_message(const std::string& stream, const std::string& group, 
                     const std::string& msg_id);
    
    // Guest PIN operations
    bool store_guest_pin(const std::string& pin, int64_t tg_user_id, int ttl_seconds);
    std::optional<int64_t> verify_guest_pin(const std::string& pin);
    void set_guest_session(int64_t tg_user_id, int ttl_seconds);
    bool is_guest_active(int64_t tg_user_id);
    
    // Health check
    bool ping();
    
private:
    std::shared_ptr<sw::redis::Redis> redis_;
    
    std::string guest_pin_key(const std::string& pin) const;
    std::string guest_session_key(int64_t tg_user_id) const;
};
