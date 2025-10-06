#include "parser.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>

ParsedCommand CommandParser::parse(const std::string& text) {
    ParsedCommand result;
    
    std::string trimmed = util::trim(text);
    if (trimmed.empty() || trimmed[0] != '/') {
        result.error = "Not a command";
        return result;
    }
    
    // Remove leading /
    trimmed = trimmed.substr(1);
    
    // Split by whitespace
    auto tokens = util::split(trimmed, ' ');
    if (tokens.empty()) {
        result.error = "Empty command";
        return result;
    }
    
    result.cmd = util::trim(tokens[0]);
    
    // Collect arguments
    for (size_t i = 1; i < tokens.size(); i++) {
        std::string arg = util::trim(tokens[i]);
        if (!arg.empty()) {
            result.args.push_back(arg);
        }
    }
    
    // Validate command
    if (!is_valid_command(result.cmd)) {
        result.error = "Unknown command: /" + result.cmd;
        return result;
    }
    
    return result;
}

bool CommandParser::is_valid_command(const std::string& cmd) {
    static const std::vector<std::string> valid_commands = {
        "start", "help", "balance", "holdings", "signals",
        "silence", "resume", "health", "add_wallet", 
        "remove_wallet", "guest"
    };
    
    return std::find(valid_commands.begin(), valid_commands.end(), cmd) 
           != valid_commands.end();
}

nlohmann::json CommandParser::to_request_json(
    const ParsedCommand& cmd,
    int64_t tg_user_id,
    const std::string& role,
    const std::string& corr_id
) {
    nlohmann::json j;
    j["type"] = "command";
    j["cmd"] = cmd.cmd;
    j["corr_id"] = corr_id;
    j["ts"] = util::current_iso8601();
    
    // Build args object
    nlohmann::json args = nlohmann::json::object();
    
    if (cmd.cmd == "signals" && !cmd.args.empty()) {
        args["window"] = cmd.args[0];
    } else if (cmd.cmd == "silence") {
        int minutes = 30; // default
        if (!cmd.args.empty()) {
            try {
                minutes = std::stoi(cmd.args[0]);
            } catch (...) {
                minutes = 30;
            }
        }
        args["minutes"] = minutes;
    } else if (cmd.cmd == "guest") {
        int minutes = 30; // default
        if (!cmd.args.empty()) {
            try {
                minutes = std::stoi(cmd.args[0]);
            } catch (...) {
                minutes = 30;
            }
        }
        args["minutes"] = minutes;
    } else if (cmd.cmd == "add_wallet" || cmd.cmd == "remove_wallet") {
        if (!cmd.args.empty()) {
            args["address"] = cmd.args[0];
        }
    } else if (cmd.cmd == "holdings") {
        args["limit"] = 10; // default
        if (!cmd.args.empty()) {
            try {
                args["limit"] = std::stoi(cmd.args[0]);
            } catch (...) {}
        }
    }
    
    j["args"] = args;
    
    // From
    j["from"] = {
        {"tg_user_id", tg_user_id},
        {"role", role}
    };
    
    return j;
}