#pragma once

#include "state.hpp"

// v1.1 Signal scores (0.0 to 1.0)
struct SignalScores {
    double S1; // Liquidity
    double S2; // Volume
    double S3; // FDV/Liq ratio
    double S4; // Momentum (m1h, m24h)
    double S5; // Drawdown structure
    double S6; // Volatility/consistency
    double S7; // Rug risk (authorities, holder concentration)
    double S8; // Spread + impact
    double S9; // Volume trend
    double S10; // Route health
    
    double N1; // Token list hygiene (binary: 1.0 or penalty)
    
    std::vector<std::string> warnings; // Gate failures
};

class SignalCalculator {
public:
    static SignalScores compute_signals(const TokenState& state);
    
private:
    // S1: Liquidity gates
    static double compute_S1(double liq_usd);
    
    // S2: Volume gates
    static double compute_S2(double vol24h_usd);
    
    // S3: FDV/Liq ratio (preferred 5-50)
    static double compute_S3(double fdv_liq_ratio);
    
    // S4: Momentum (m1h +1..+12%, m24h +2..+60%)
    static double compute_S4(double m1h, double m24h);
    
    // S5: Drawdown structure (bounce from support)
    static double compute_S5(const TokenState& state);
    
    // S6: Volatility check (not too chaotic)
    static double compute_S6(const TokenState& state);
    
    // S7: Rug risk heuristics
    static double compute_S7(const TokenState& state);
    
    // S8: Execution cost (spread + impact)
    static double compute_S8(double spread_pct, double impact_pct);
    
    // S9: Volume trend
    static double compute_S9(const TokenState& state);
    
    // S10: Route health
    static double compute_S10(const MarketData::Route& route);
    
    // N1: Token list hygiene
    static double compute_N1(const std::string& symbol);
};
