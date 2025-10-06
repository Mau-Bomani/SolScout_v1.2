#include "scoring.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

ConfidenceScorer::ConfidenceScorer(const ScoringWeights& weights)
    : weights_(weights) {}

ConfidenceResult ConfidenceScorer::compute_confidence(const TokenState& state,
                                                      const SignalScores& signals) const {
    ConfidenceResult result;
    
ConfidenceResult ConfidenceScorer::compute_confidence(const TokenState& state,
                                                      const SignalScores& signals) const {
    ConfidenceResult result;
    
    // Compute raw weighted score (R)
    result.raw_score = 
        signals.S1 * weights_.w_S1 +
        signals.S2 * weights_.w_S2 +
        signals.S3 * weights_.w_S3 +
        signals.S4 * weights_.w_S4 +
        signals.S5 * weights_.w_S5 +
        signals.S6 * weights_.w_S6 +
        signals.S7 * weights_.w_S7 +
        signals.S8 * weights_.w_S8 +
        signals.S9 * weights_.w_S9 +
        signals.S10 * weights_.w_S10;
    
    // Scale to 0-100
    result.raw_score *= 100.0;
    
    // Compute data quality factor
    result.data_quality = compute_data_quality(signals, state);
    
    // Check for young-and-risky
    result.young_and_risky = (state.latest.age_hours < 72.0 && signals.S7 < 0.6);
    
    // Check for rug cap
    result.rug_cap_applied = (signals.S7 < 0.3);
    if (result.rug_cap_applied) {
        result.raw_score = std::min(result.raw_score, 55.0);
    }
    
    // Compute penalties
    result.penalties = compute_penalties(state, signals);
    
    // Token list hygiene penalty (N1)
    if (signals.N1 < 1.0) {
        result.penalties += 10.0;
        result.reasons.push_back("Not on widely mirrored lists");
    }
    
    // Final confidence = max(0, R - P)
    result.final_confidence = std::max(0.0, result.raw_score - result.penalties);
    
    // DQ gate: force Heads-up if DQ < 0.7
    result.dq_forced_headsup = (result.data_quality < 0.7);
    
    return result;
}

double ConfidenceScorer::compute_data_quality(const SignalScores& signals, 
                                               const TokenState& state) const {
    double dq = 1.0;
    
    // Subtract 0.08 for each missing/reconstructed key input
    if (signals.S1 < 0.1) dq -= 0.08; // Missing liquidity
    if (signals.S2 < 0.1) dq -= 0.08; // Missing volume
    if (signals.S4 < 0.1) dq -= 0.08; // Missing momentum
    if (state.latest.bar_5m.v_usd == 0) dq -= 0.08; // No 5m bar
    if (state.latest.bar_15m.v_usd == 0) dq -= 0.08; // No 15m bar
    if (state.latest.dq == "degraded") dq -= 0.08; // Upstream degradation
    
    return std::max(0.0, dq);
}

double ConfidenceScorer::compute_penalties(const TokenState& state, 
                                            const SignalScores& signals) const {
    double penalties = 0.0;
    
    // Age penalty for very young tokens
    if (state.latest.age_hours < 24.0) {
        penalties += 15.0;
    } else if (state.latest.age_hours < 48.0) {
        penalties += 5.0;
    }
    
    // FDV/Liq penalty
    // (Would need actual FDV data - using placeholder)
    
    // Execution cost penalty
    if (state.latest.spread_pct > 1.5) {
        penalties += 5.0;
    }
    if (state.latest.impact_1pct_pct > 1.0) {
        penalties += 5.0;
    }
    
    // Volume consistency penalty
    if (signals.S9 < 0.5) {
        penalties += 3.0;
    }
    
    return penalties;
}

std::string ConfidenceScorer::determine_band(double confidence, 
                                              const ConfidenceResult& result,
                                              int regime_adjusted_threshold) const {
    // DQ gate overrides everything
    if (result.dq_forced_headsup) {
        return "heads_up";
    }
    
    // High conviction: C ≥ 85, no hard risk flags
    if (confidence >= 85.0 && !result.rug_cap_applied && !result.young_and_risky) {
        return "high_conviction";
    }
    
    // Actionable: C ≥ regime-adjusted threshold
    if (confidence >= regime_adjusted_threshold) {
        return "actionable";
    }
    
    // Heads-up: 60-69
    if (confidence >= 60.0) {
        return "heads_up";
    }
    
    // Below threshold
    return "none";
}