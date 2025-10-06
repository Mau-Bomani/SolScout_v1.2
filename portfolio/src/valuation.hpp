#pragma once

#include <string>
#include <vector>
#include <optional>

enum class ValuationTag {
    CG,           // CoinGecko listed
    DEX,          // DEX with >= 75k liquidity
    EST_50,       // 25k-75k liquidity with 50% haircut
    NA            // Not valueable
};

struct Holding {
    std::string mint;
    std::string symbol;
    double amount;
    std::optional<double> usd_price;
    std::optional<double> usd_value;
    ValuationTag tag;
    
    std::string tag_string() const;
};

struct PortfolioSummary {
    double total_usd;
    int included_count;
    int excluded_count;
    double haircut_subtotal_usd;
    std::vector<Holding> holdings;
    std::string notes;
};

class Valuator {
public:
    explicit Valuator(double dust_min_usd, int haircut_pct);
    
    PortfolioSummary value_portfolio(const std::vector<Holding>& holdings);
    
private:
    double dust_min_usd_;
    int haircut_pct_;
    
    bool is_dust(const Holding& holding) const;
};