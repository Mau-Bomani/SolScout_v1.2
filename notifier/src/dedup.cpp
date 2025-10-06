#include "dedup.hpp"
#include <spdlog/spdlog.h>

DedupManager::DedupManager(std::shared_ptr<sw::redis::Redis> redis, int ttl_seconds)
    : redis_(redis), ttl_seconds_(ttl_seconds) {}

std::string DedupManager::make_key(const std::string& symbol, const std::string& band,
                                   const std::string& reason_hash) const {
    return "notifier:dedup:" + symbol + ":" + band + ":" + reason_hash;
}

bool DedupManager::is_duplicate(const std::string& symbol, const std::string& band,
                                const std::string& reason_hash) {
    try {
        std::string key = make_key(symbol, band, reason_hash);
        auto val = redis_->get(key);
        return val.has_value();
    } catch (const std::exception& e) {
        spdlog::error("Failed to check dedup: {}", e.what());
        return false;
    }
}

void DedupManager::record(const std::string& symbol, const std::string& band,
                          const std::string& reason_hash) {
    try {
        std::string key = make_key(symbol, band, reason_hash);
        redis_->setex(key, ttl_seconds_, "1");
    } catch (const std::exception& e) {
        spdlog::error("Failed to record dedup: {}", e.what());
    }
}

// util.hpp & cpp
#pragma once
#include <string>
#include <chrono>
#include <functional>

namespace util {
    std::string current_iso8601();
    int64_t current_timestamp_ms();
    std::string hash_reasons(const std::vector<std::string>& reasons);
}

#include "util.hpp"
#include <sstream>
#include <iomanip>

namespace util {

std::string current_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto itt = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&itt), "%FT%TZ");
    return ss.str();
}

int64_t current_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string hash_reasons(const std::vector<std::string>& reasons) {
    std::string combined;
    for (const auto& r : reasons) {
        combined += r + "|";
    }
    
    std::hash<std::string> hasher;
    size_t hash_val = hasher(combined);
    
    std::ostringstream ss;
    ss << std::hex << hash_val;
    return ss.str();
}

} // namespace util