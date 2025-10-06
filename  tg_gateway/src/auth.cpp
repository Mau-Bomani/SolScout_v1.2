#include "auth.hpp"
#include <vector>
#include <algorithm>

AuthResult Authenticator::authenticate(int64_t tg_user_id, const std::string& pin) const {
    AuthResult result;
    
    // Check if owner
    if (tg_user_id == owner_id_) {
        result.role = UserRole::Owner;
        result.authorized = true;
        return result;
    }
    
    // Guest authentication would check PIN against Redis
    // For now, treat all non-owners as unknown
    // The Redis-based guest system will be implemented in redis_bus
    result.role = UserRole::Unknown;
    result.authorized = false;
    result.message = "Access denied. This bot is private.";
    
    return result;
}

bool Authenticator::is_command_allowed(const std::string& cmd, UserRole role) const {
    // Owner can do anything
    if (role == UserRole::Owner) {
        return true;
    }
    
    // Guest can only do read-only commands
    if (role == UserRole::Guest) {
        return !is_owner_only_command(cmd);
    }
    
    // Unknown users: only /start allowed (for guest pairing)
    return cmd == "start" || cmd == "help";
}

bool Authenticator::is_owner_only_command(const std::string& cmd) {
    static const std::vector<std::string> owner_only = {
        "silence", "resume", "add_wallet", "remove_wallet", "guest"
    };
    
    return std::find(owner_only.begin(), owner_only.end(), cmd) != owner_only.end();
}