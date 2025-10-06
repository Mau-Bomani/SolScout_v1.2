#include "mute_state.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>

MuteState::MuteState(std::shared_ptr<sw::redis::Redis> redis) : redis_(redis) {}

std::string MuteState::mute_key(int64_t user_id) const {
    return "notifier:mute:" + std::to_string(user_id);
}

void MuteState::set_mute(int64_t user_id, int minutes) {
    try {
        std::string key = mute_key(user_id);
        redis_->setex(key, minutes * 60, "1");
        spdlog::info("Muted alerts for user {} for {} minutes", user_id, minutes);
    } catch (const std::exception& e) {
        spdlog::error("Failed to set mute: {}", e.what());
    }
}

void MuteState::clear_mute(int64_t user_id) {
    try {
        std::string key = mute_key(user_id);
        redis_->del(key);
        spdlog::info("Cleared mute for user {}", user_id);
    } catch (const std::exception& e) {
        spdlog::error("Failed to clear mute: {}", e.what());
    }
}

bool MuteState::is_muted(int64_t user_id) {
    try {
        std::string key = mute_key(user_id);
        auto val = redis_->get(key);
        return val.has_value();
    } catch (const std::exception& e) {
        spdlog::error("Failed to check mute: {}", e.what());
        return false;
    }
}

int MuteState::get_remaining_minutes(int64_t user_id) {
    try {
        std::string key = mute_key(user_id);
        auto ttl = redis_->ttl(key);
        return ttl > 0 ? (ttl / 60) : 0;
    } catch (const std::exception& e) {
        return 0;
    }
}