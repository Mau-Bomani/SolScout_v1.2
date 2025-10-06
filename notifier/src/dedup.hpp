#pragma once
#include <string>
#include <memory>
#include <sw/redis++/redis++.h>

class DedupManager {
public:
    DedupManager(std::shared_ptr<sw::redis::Redis> redis, int ttl_seconds);
    
    bool is_duplicate(const std::string& symbol, const std::string& band,
                     const std::string& reason_hash);
    void record(const std::string& symbol, const std::string& band,
               const std::string& reason_hash);
    
private:
    std::shared_ptr<sw::redis::Redis> redis_;
    int ttl_seconds_;
    
    std::string make_key(const std::string& symbol, const std::string& band,
                        const std::string& reason_hash) const;
};