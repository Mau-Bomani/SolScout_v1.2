#include <catch2/catch_test_macros.hpp>
#include "../src/parser.hpp"
#include "../src/auth.hpp"
#include "../src/rate_limiter.hpp"

TEST_CASE("Command parsing", "[parser]") {
    SECTION("Valid commands") {
        auto cmd = CommandParser::parse("/start");
        REQUIRE(cmd.is_valid());
        REQUIRE(cmd.cmd == "start");
        REQUIRE(cmd.args.empty());
        
        auto cmd2 = CommandParser::parse("/signals 24h");
        REQUIRE(cmd2.is_valid());
        REQUIRE(cmd2.cmd == "signals");
        REQUIRE(cmd2.args.size() == 1);
        REQUIRE(cmd2.args[0] == "24h");
        
        auto cmd3 = CommandParser::parse("/silence 30");
        REQUIRE(cmd3.is_valid());
        REQUIRE(cmd3.cmd == "silence");
        REQUIRE(cmd3.args.size() == 1);
        REQUIRE(cmd3.args[0] == "30");
    }
    
    SECTION("Invalid commands") {
        auto cmd = CommandParser::parse("hello");
        REQUIRE_FALSE(cmd.is_valid());
        
        auto cmd2 = CommandParser::parse("/unknown");
        REQUIRE_FALSE(cmd2.is_valid());
    }
    
    SECTION("Request JSON generation") {
        auto cmd = CommandParser::parse("/balance");
        auto json = CommandParser::to_request_json(cmd, 123456, "owner", "test-corr-id");
        
        REQUIRE(json["type"] == "command");
        REQUIRE(json["cmd"] == "balance");
        REQUIRE(json["corr_id"] == "test-corr-id");
        REQUIRE(json["from"]["tg_user_id"] == 123456);
        REQUIRE(json["from"]["role"] == "owner");
    }
}

TEST_CASE("Authentication", "[auth]") {
    int64_t owner_id = 123456789;
    Authenticator auth(owner_id);
    
    SECTION("Owner authentication") {
        auto result = auth.authenticate(owner_id);
        REQUIRE(result.role == UserRole::Owner);
        REQUIRE(result.authorized);
    }
    
    SECTION("Unknown user") {
        auto result = auth.authenticate(999999999);
        REQUIRE(result.role == UserRole::Unknown);
        REQUIRE_FALSE(result.authorized);
    }
    
    SECTION("Command permissions") {
        REQUIRE(auth.is_command_allowed("balance", UserRole::Owner));
        REQUIRE(auth.is_command_allowed("balance", UserRole::Guest));
        REQUIRE_FALSE(auth.is_command_allowed("silence", UserRole::Guest));
        REQUIRE(auth.is_command_allowed("silence", UserRole::Owner));
    }
}

TEST_CASE("Rate limiting", "[rate_limiter]") {
    RateLimiter limiter(5); // 5 messages per minute
    int64_t user_id = 123456;
    
    SECTION("Under limit") {
        for (int i = 0; i < 5; i++) {
            REQUIRE(limiter.check_and_record(user_id));
        }
    }
    
    SECTION("Over limit") {
        for (int i = 0; i < 5; i++) {
            limiter.check_and_record(user_id);
        }
        REQUIRE_FALSE(limiter.check_and_record(user_id));
    }
}