#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

struct ParsedCommand {
    std::string cmd;
    std::vector<std::string> args;
    std::optional<std::string> error;
    
    bool is_valid() const { return !error.has_value(); }
};

class CommandParser {
public:
    static ParsedCommand parse(const std::string& text);
    static nlohmann::json to_request_json(
        const ParsedCommand& cmd,
        int64_t tg_user_id,
        const std::string& role,
        const std::string& corr_id
    );
    
private:
    static bool is_valid_command(const std::string& cmd);
};
