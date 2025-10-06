#include <catch2/catch_test_macros.hpp>
#include "../src/normalize.hpp"

TEST_CASE("Pool normalization", "[normalize]") {
    SECTION("Normalize valid pool data") {
        nlohmann::json raw = {
            {"address", "pool123"},
            {"mint_base", "base_mint"},
            {"mint_quote", "quote_mint"},
            {"price", 0.5},
            {"liquidity_usd", 100000.0},
            {"volume_24h_usd", 50000.0},
            {"reserve_base", 1000000.0},
            {"reserve_quote", 500000.0}
        };
        
        auto pool = Normalizer::normalize_pool(raw, "raydium");
        
        REQUIRE(pool.address == "pool123");
        REQUIRE(pool.dex == "raydium");
        REQUIRE(pool.price == 0.5);
        REQUIRE(pool.liq_usd == 100000.0);
        REQUIRE(pool.dq == "ok");
    }
    
    SECTION("Missing data marks as degraded") {
        nlohmann::json raw = {
            {"address", "pool123"},
            {"price", 0.0},
            {"liquidity_usd", 0.0}
        };
        
        auto pool = Normalizer::normalize_pool(raw, "orca");
        
        REQUIRE(pool.dq == "degraded");
    }
}