#pragma once
#include <string>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>

class RedisBus {
public:
    explicit RedisBus(const std::string& redis_url);
    
    void create_consumer_group(const std::string& stream, const std::string& group);
    std::vector<std::pair<std::string, nlohmann::json>>
        read_alerts(const std::string& stream, const std::string& group,
                   const std::string& consumer, int count, int block_ms);
    std::vector<std::pair<std::string, nlohmann::json>>
        read_commands(const std::string& stream, const std::string& group,
                     const std::string& consumer, int count, int block_ms);
    void ack_message(const std::string& stream, const std::string& group,
                    const std::string& msg_id);
    void publish_outbound_alert(const std::string& stream, const nlohmann::json& data);
    void publish_reply(const std::string& stream, const nlohmann::json& data);
    bool ping();
    
    std::shared_ptr<sw::redis::Redis> get_redis() { return redis_; }
    
private:
    std::shared_ptr<sw::redis::Redis> redis_;
};