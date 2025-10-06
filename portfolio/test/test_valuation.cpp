#include <catch2/catch_test_macros.hpp>
#include "../src/valuation.hpp"
#include "../src/util.hpp"

TEST_CASE("Valuation logic", "[valuation]") {
    Valuator valuator(0.50, 50); // $0.50 dust threshold, 50% haircut
    
    SECTION("CoinGecko tagged holdings included in total") {
        std::vector<Holding> holdings;
        
        Holding h1;
        h1.mint = "mint1";
        h1.symbol = "SOL";
        h1.amount = 10.0;
        h1.usd_price = 100.0;
        h1.usd_value = 1000.0;
        h1.tag = ValuationTag::CG;
        holdings.push_back(h1);
        
        auto summary = valuator.value_portfolio(holdings);
        
        REQUIRE(summary.total_usd == 1000.0);
        REQUIRE(summary.included_count == 1);
        REQUIRE(summary.excluded_count == 0);
    }
    
    SECTION("DEX tagged holdings included") {
        std::vector<Holding> holdings;
        
        Holding h1;
        h1.mint = "mint1";
        h1.symbol = "TOKEN";
        h1.amount = 100.0;
        h1.usd_price = 5.0;
        h1.usd_value = 500.0;
        h1.tag = ValuationTag::DEX;
        holdings.push_back(h1);
        
        auto summary = valuator.value_portfolio(holdings);
        
        REQUIRE(summary.total_usd == 500.0);
        REQUIRE(summary.included_count == 1);
    }
    
    SECTION("EST_50 tagged holdings go to haircut subtotal") {
        std::vector<Holding> holdings;
        
        Holding h1;
        h1.mint = "mint1";
        h1.symbol = "LOWLIQ";
        h1.amount = 100.0;
        h1.usd_price = 2.0;
        h1.usd_value = 200.0;
        h1.tag = ValuationTag::EST_50;
        holdings.push_back(h1);
        
        auto summary = valuator.value_portfolio(holdings);
        
        REQUIRE(summary.total_usd == 0.0);
        REQUIRE(summary.haircut_subtotal_usd == 100.0); // 50% of 200
        REQUIRE(summary.included_count == 0);
    }
    
    SECTION("NA tagged holdings excluded") {
        std::vector<Holding> holdings;
        
        Holding h1;
        h1.mint = "mint1";
        h1.symbol = "UNKNOWN";
        h1.amount = 100.0;
        h1.tag = ValuationTag::NA;
        holdings.push_back(h1);
        
        auto summary = valuator.value_portfolio(holdings);
        
        REQUIRE(summary.total_usd == 0.0);
        REQUIRE(summary.excluded_count == 1);
    }
    
    SECTION("Dust filtering") {
        std::vector<Holding> holdings;
        
        Holding h1;
        h1.mint = "mint1";
        h1.symbol = "DUST";
        h1.amount = 0.01;
        h1.usd_price = 0.01;
        h1.usd_value = 0.0001;
        h1.tag = ValuationTag::CG;
        holdings.push_back(h1);
        
        auto summary = valuator.value_portfolio(holdings);
        
        // Should be filtered out as dust
        REQUIRE(summary.total_usd == 0.0);
        REQUIRE(summary.included_count == 0);
    }
}

TEST_CASE("Utility functions", "[util]") {
    SECTION("Solana address validation") {
        REQUIRE(util::is_valid_solana_address("7xKXtg2CW87d97TXJSDpbD5jBkheTqA83TZRuJosgAsU"));
        REQUIRE_FALSE(util::is_valid_solana_address("invalid"));
        REQUIRE_FALSE(util::is_valid_solana_address(""));
        REQUIRE_FALSE(util::is_valid_solana_address("tooshort"));
    }
    
    SECTION("DSN redaction") {
        std::string dsn = "postgresql://user:password@localhost:5432/db";
        std::string redacted = util::redact_dsn(dsn);
        
        REQUIRE(redacted.find("password") == std::string::npos);
        REQUIRE(redacted.find("***") != std::string::npos);
    }
}