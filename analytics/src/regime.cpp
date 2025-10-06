#include "regime.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

RegimeAssessment RegimeDetector::assess_regime(StateManager& state_mgr) {
    RegimeAssessment result;
    
    // Compute 3 regime indicators
    result.sol_positive = (compute_sol_24h_return(state_mgr) > 0);
    result.median_positive = (compute_median_24h_return(state_mgr) > 0);
    result.above_vwap_majority = (compute_above_vwap_ratio(state_mgr) > 0.5);
    
    // Count positive indicators
    int positive_count = 0;
    if (result.sol_positive) positive_count++;
    if (result.median_positive) positive_count++;
    if (result.above_vwap_majority) positive_count++;
    
    // Determine regime
    if (positive_count >= 2) {
        result.regime = MarketRegime::RiskOn;
        result.threshold_adjustment = -10;
        result.size_adjustment_pct = 30.0;
        result.description = "Risk-On: Lower threshold, larger size";
    } else if (positive_count == 0) {
        result.regime = MarketRegime::RiskOff;
        result.threshold_adjustment = 10;
        result.size_adjustment_pct = -30.0;
        result.description = "Risk-Off: Higher threshold, smaller size";
    } else {
        result.regime = MarketRegime::Neutral;
        result.threshold_adjustment = 0;
        result.size_adjustment_pct = 0.0;
        result.description = "Neutral: Base parameters";
    }
    
    spdlog::info("Regime: {} (SOL={}, median={}, VWAP={})",
                 result.description,
                 result.sol_positive ? "+" : "-",
                 result.median_positive ? "+" : "-",
                 result.above_vwap_majority ? "+" : "-");
    
    return result;
}

double RegimeDetector::compute_sol_24h_return(StateManager& state_mgr) {
    auto sol_state = state_mgr.get_token("SOL");
    if (!sol_state) return 0.0;
    
    return sol_state->compute_m24h();
}

double RegimeDetector::compute_median_24h_return(StateManager& state_mgr) {
    std::vector<double> returns;
    
    auto symbols = state_mgr.get_all_symbols();
    for (const auto& sym : symbols) {
        auto state = state_mgr.get_token(sym);
        if (state) {
            returns.push_back(state->compute_m24h());
        }
    }
    
    if (returns.empty()) return 0.0;
    
    std::sort(returns.begin(), returns.end());
    return returns[returns.size() / 2];
}

double RegimeDetector::compute_above_vwap_ratio(StateManager& state_mgr) {
    int above_count = 0;
    int total_count = 0;
    
    auto symbols = state_mgr.get_all_symbols();
    for (const auto& sym : symbols) {
        auto state = state_mgr.get_token(sym);
        if (!state) continue;
        
        // Compute 24h VWAP proxy
        double vwap_proxy = 0.0;
        double vol_sum = 0.0;
        
        for (const auto& md : state->history_24h) {
            vwap_proxy += md.price * md.bar_5m.v_usd;
            vol_sum += md.bar_5m.v_usd;
        }
        
        if (vol_sum > 0) {
            vwap_proxy /= vol_sum;
            
            if (state->latest.price > vwap_proxy) {
                above_count++;
            }
            total_count++;
        }
    }
    
    if (total_count == 0) return 0.5;
    
    return static_cast<double>(above_count) / total_count;
}