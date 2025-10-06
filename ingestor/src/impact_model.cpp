#include "impact_model.hpp"
#include <cmath>
#include <algorithm>

double ImpactModel::calculate_1pct_impact(double reserve_base, double reserve_quote, 
                                          double liquidity_usd) {
    if (liquidity_usd <= 0 || reserve_base <= 0 || reserve_quote <= 0) {
        return 999.0; // Very high impact for invalid pools
    }
    
    // For 1% of liquidity purchase
    double purchase_usd = liquidity_usd * 0.01;
    
    // Constant product: k = x * y
    double k = reserve_base * reserve_quote;
    
    // After buying, new quote reserve
    double new_reserve_quote = reserve_quote + purchase_usd;
    
    // Solve for new base reserve: new_base = k / new_quote
    double new_reserve_base = k / new_reserve_quote;
    
    // Tokens received
    double tokens_received = reserve_base - new_reserve_base;
    
    // Price without impact
    double price_before = reserve_quote / reserve_base;
    
    // Effective price paid
    double effective_price = purchase_usd / tokens_received;
    
    // Impact percentage
    double impact_pct = ((effective_price - price_before) / price_before) * 100.0;
    
    return std::max(0.0, impact_pct);
}

double ImpactModel::estimate_spread_pct(double reserve_base, double reserve_quote) {
    if (reserve_base <= 0 || reserve_quote <= 0) {
        return 10.0; // High spread for invalid pools
    }
    
    // Simplified spread estimation based on pool depth
    // In reality would need tick data from the DEX
    double liquidity_score = std::sqrt(reserve_base * reserve_quote);
    
    // Inverse relationship: larger pools = tighter spreads
    double spread = 100.0 / std::max(1.0, liquidity_score / 100000.0);
    
    return std::min(10.0, std::max(0.01, spread));
}