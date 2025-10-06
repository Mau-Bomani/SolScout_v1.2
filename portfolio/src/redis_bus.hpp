#pragma once

#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>

class RedisBus {
public:
    explicit RedisBus(const std::string& redis_url);
    
    void create_consumer_group(const std::string& stream, const std::string& group);
    
    std::vector<std::pair<std::string, nlohmann::json>>
        read_commands(const std::string& stream, const std::string& group,
                     const std::string& consumer, int count = 10, int block_ms = 1000);
    
    void publish_reply(const std::string& stream, const nlohmann::json& data);
    void publish_audit(const std::string& stream, const nlohmann::json& data);
    
    void ack_message(const std::string& stream, const std::string& group,
                    const std::string& msg_id);
    
    bool ping();
    
private:
    std::shared_ptr<sw::redis::Redis> redis_;
};