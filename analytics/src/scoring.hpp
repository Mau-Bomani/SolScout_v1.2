#pragma once

#include "signals.hpp"
#include "state.hpp"

// v1.1 weights for confidence calculation
struct ScoringWeights {
    double w_S1 = 0.15;  // Liquidity
    double w_S2 = 0.12;  // Volume
    double w_S3 = 0.08;  // FDV/Liq
    double w_S4 = 0.18;  // Momentum
    double w_S5 = 0.10;  // Structure
    double w_S6 = 0.08;  // Volatility
    double w_S7 = 0.12;  // Rug risk
    double w_S8 = 0.10;  // Execution
    double w_S9 = 0.05;  // Volume trend
    double w_S10 = 0.02; // Route
};

struct ConfidenceResult {
    double raw_score;        // R = weighted sum before penalties
    double data_quality;     // DQ factor (0.7-1.0)
    double penalties;        // P = total penalties
    double final_confidence; // C = max(0, R - P)
    
    bool young_and_risky;    // Age <72h + unknown authorities
    bool rug_cap_applied;    // S7 <0.3 → cap at 55
    bool dq_forced_headsup;  // DQ <0.7 → force Heads-up
    
    std::string band;        // "heads_up", "actionable", "high_conviction"
    std::vector<std::string> reasons;
};

class ConfidenceScorer {
public:
    explicit ConfidenceScorer(const ScoringWeights& weights = ScoringWeights());
    
    ConfidenceResult compute_confidence(const TokenState& state, 
                                       const SignalScores& signals) const;
    
private:
    ScoringWeights weights_;
    
    double compute_data_quality(const SignalScores& signals, const TokenState& state) const;
    double compute_penalties(const TokenState& state, const SignalScores& signals) const;
    std::string determine_band(double confidence, const ConfidenceResult& result, 
                               int regime_adjusted_threshold) const;
};