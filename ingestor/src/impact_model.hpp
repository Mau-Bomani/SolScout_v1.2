#pragma once

class ImpactModel {
public:
    // Calculate price impact for buying 1% of liquidity using constant product (x*y=k)
    static double calculate_1pct_impact(double reserve_base, double reserve_quote, 
                                       double liquidity_usd);
    
    // Estimate spread percentage from reserves
    static double estimate_spread_pct(double reserve_base, double reserve_quote);
};
