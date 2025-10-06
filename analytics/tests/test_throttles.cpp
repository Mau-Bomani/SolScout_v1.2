#include <catch2/catch_test_macros.hpp>
#include "../src/throttles.hpp"

TEST_CASE("Throttle manager", "[throttles]") {
    ThrottleManager mgr;
    
    SECTION("Cooldown prevents duplicate alerts") {
        REQUIRE(mgr.check_token_cooldown("SOL", "actionable", 6));
        
        mgr.record_alert("SOL", "actionable", "hash123");
        
        REQUIRE_FALSE(mgr.check_token_cooldown("SOL", "actionable", 6));
    }
    
    SECTION("Global limit enforced") {
        for (int i = 0; i < 5; i++) {
            REQUIRE(mgr.check_global_limit(5));
            mgr.record_global_alert();
        }
        
        REQUIRE_FALSE(mgr.check_global_limit(5));
    }
    
    SECTION("Duplicate detection by reason hash") {
        mgr.record_alert("SOL", "actionable", "hash456");
        
        REQUIRE(mgr.is_duplicate("SOL", "hash456", 6));
        REQUIRE_FALSE(mgr.is_duplicate("SOL", "hash789", 6));
    }
}