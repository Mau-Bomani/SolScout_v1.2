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
        read_market_updates(const std::string& stream, const std::string& group,
                           const std::string& consumer, int count, int block_ms);
    void ack_message(const std::string& stream, const std::string& group,
                    const std::string& msg_id);
    void publish_alert(const std::string& stream, const nlohmann::json& data);
    bool ping();
    
private:
    std::shared_ptr<sw::redis::Redis> redis_;
};