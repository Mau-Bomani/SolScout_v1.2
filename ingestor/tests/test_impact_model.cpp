#include <catch2/catch_test_macros.hpp>
#include "../src/impact_model.hpp"

TEST_CASE("Impact model", "[impact_model]") {
    SECTION("Calculate 1% impact") {
        double reserve_base = 1000000.0;
        double reserve_quote = 100000.0;
        double liq_usd = 200000.0;
        
        double impact = ImpactModel::calculate_1pct_impact(
            reserve_base, reserve_quote, liq_usd);
        
        REQUIRE(impact >= 0.0);
        REQUIRE(impact < 100.0); // Sanity check
    }
    
    SECTION("Zero reserves return high impact") {
        double impact = ImpactModel::calculate_1pct_impact(0, 0, 100000);
        REQUIRE(impact > 100.0);
    }
    
    SECTION("Estimate spread from reserves") {
        double spread = ImpactModel::estimate_spread_pct(1000000, 100000);
        REQUIRE(spread > 0.0);
        REQUIRE(spread < 10.0);
    }
}
