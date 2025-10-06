#include "metadata_cache.hpp"
#include <spdlog/spdlog.h>

MetadataCache::MetadataCache(int ttl_seconds) : ttl_seconds_(ttl_seconds) {}

bool MetadataCache::is_expired(const TokenMetadata& metadata) const {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - metadata.cached_at).count();
    return age > ttl_seconds_;
}

TokenMetadata MetadataCache::get_or_fetch(const std::string& mint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(mint);
    if (it != cache_.end() && !is_expired(it->second)) {
        return it->second;
    }
    
    // Fetch from chain (simplified for now)
    auto metadata = fetch_from_chain(mint);
    cache_[mint] = metadata;
    
    return metadata;
}

void MetadataCache::update(const std::string& mint, const TokenMetadata& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[mint] = metadata;
}

TokenMetadata MetadataCache::fetch_from_chain(const std::string& mint) {
    // Simplified - in production would query on-chain metadata
    TokenMetadata metadata;
    metadata.mint = mint;
    metadata.symbol = "UNKNOWN";
    metadata.name = "Unknown Token";
    metadata.decimals = 9; // SOL default
    metadata.is_nft = false;
    metadata.cached_at = std::chrono::steady_clock::now();
    
    spdlog::debug("Fetched metadata for mint {}", mint.substr(0, 8) + "...");
    
    return metadata;
}