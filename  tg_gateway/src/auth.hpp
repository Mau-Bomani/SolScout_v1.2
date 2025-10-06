#pragma once

#include <string>
#include <cstdint>
#include <optional>

enum class UserRole {
    Owner,
    Guest,
    Unknown
};

struct AuthResult {
    UserRole role;
    bool authorized;
    std::optional<std::string> message;
    
    std::string role_string() const {
        switch (role) {
            case UserRole::Owner: return "owner";
            case UserRole::Guest: return "guest";
            default: return "unknown";
        }
    }
};

class Authenticator {
public:
    explicit Authenticator(int64_t owner_id) : owner_id_(owner_id) {}
    
    AuthResult authenticate(int64_t tg_user_id, const std::string& pin = "") const;
    bool is_command_allowed(const std::string& cmd, UserRole role) const;
    
private:
    int64_t owner_id_;
    
    static bool is_owner_only_command(const std::string& cmd);
};