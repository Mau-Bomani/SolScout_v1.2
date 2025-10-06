#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

struct TokenMetadata {
    std::string mint;
    std::string symbol;
    std::string name;
    uint8_t decimals;
    bool is_nft;
    std::chrono::steady_clock::time_point cached_at;
};

class MetadataCache {
public:
    explicit MetadataCache(int ttl_seconds = 3600);
    
    TokenMetadata get_or_fetch(const std::string& mint);
    void update(const std::string& mint, const TokenMetadata& metadata);
    
private:
    int ttl_seconds_;
    std::mutex mutex_;
    std::unordered_map<std::string, TokenMetadata> cache_;
    
    bool is_expired(const TokenMetadata& metadata) const;
    TokenMetadata fetch_from_chain(const std::string& mint);
};