#pragma once

#include "state.hpp"
#include <vector>

enum class MarketRegime {
    RiskOn,
    RiskOff,
    Neutral
};

struct RegimeAssessment {
    MarketRegime regime;
    int threshold_adjustment;  // +10 for risk-off, -10 for risk-on
    double size_adjustment_pct; // +30% or -30%
    
    bool sol_positive;
    bool median_positive;
    bool above_vwap_majority;
    
    std::string description;
};

class RegimeDetector {
public:
    static RegimeAssessment assess_regime(StateManager& state_mgr);
    
private:
    static double compute_sol_24h_return(StateManager& state_mgr);
    static double compute_median_24h_return(StateManager& state_mgr);
    static double compute_above_vwap_ratio(StateManager& state_mgr);
};