#include <catch2/catch_test_macros.hpp>
#include "../src/bar_synth.hpp"

TEST_CASE("Bar synthesis", "[bar_synth]") {
    BarSynthesizer synth(300); // 5 minute bars
    
    SECTION("Empty synthesizer returns empty bars") {
        auto bars = synth.get_completed_bars();
        REQUIRE(bars.empty());
    }
    
    SECTION("Single tick creates incomplete bar") {
        PriceTick tick;
        tick.price = 100.0;
        tick.volume_usd = 1000.0;
        tick.timestamp_ms = 1000000000000; // Some timestamp
        
        synth.add_tick(tick);
        
        auto current = synth.get_current_bar();
        REQUIRE(current.open == 100.0);
        REQUIRE(current.close == 100.0);
    }
    
    SECTION("Multiple ticks compute OHLC correctly") {
        int64_t base_ts = 1000000000000;
        
        PriceTick t1{100.0, 500.0, base_ts};
        PriceTick t2{110.0, 600.0, base_ts + 60000};
        PriceTick t3{95.0, 400.0, base_ts + 120000};
        PriceTick t4{105.0, 500.0, base_ts + 180000};
        
        synth.add_tick(t1);
        synth.add_tick(t2);
        synth.add_tick(t3);
        synth.add_tick(t4);
        
        auto current = synth.get_current_bar();
        
        REQUIRE(current.open == 100.0);
        REQUIRE(current.high == 110.0);
        REQUIRE(current.low == 95.0);
        REQUIRE(current.close == 105.0);
        REQUIRE(current.volume_usd == 2000.0);
    }
    
    SECTION("Sparse data marks bar as degraded") {
        PriceTick tick{100.0, 1000.0, 1000000000000};
        synth.add_tick(tick);
        
        auto current = synth.get_current_bar();
        REQUIRE(current.degraded == true);
    }
}
