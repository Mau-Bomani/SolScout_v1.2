#include "signals.hpp"
#include <cmath>
#include <algorithm>

SignalScores SignalCalculator::compute_signals(const TokenState& state) {
    SignalScores scores;
    
    const auto& md = state.latest;
    
    scores.S1 = compute_S1(md.liq_usd);
    scores.S2 = compute_S2(md.vol24h_usd);
    scores.S3 = compute_S3(100.0); // FDV/Liq - would need FDV data
    scores.S4 = compute_S4(state.compute_m1h(), state.compute_m24h());
    scores.S5 = compute_S5(state);
    scores.S6 = compute_S6(state);
    scores.S7 = compute_S7(state);
    scores.S8 = compute_S8(md.spread_pct, md.impact_1pct_pct);
    scores.S9 = compute_S9(state);
    scores.S10 = compute_S10(md.route);
    scores.N1 = compute_N1(state.symbol);
    
    return scores;
}

double SignalCalculator::compute_S1(double liq_usd) {
    // Hard floor: 150k for Actionable
    // 25k-150k: Heads-up only
    if (liq_usd < 25000) return 0.0;
    if (liq_usd < 150000) return 0.5; // Partial score
    if (liq_usd >= 150000) return 1.0;
    
    // Smooth curve for higher liquidity
    return std::min(1.0, liq_usd / 150000.0);
}

double SignalCalculator::compute_S2(double vol24h_usd) {
    // Hard floor: 500k for Actionable
    // 50k-500k: Heads-up only
    if (vol24h_usd < 50000) return 0.0;
    if (vol24h_usd < 500000) return 0.5;
    if (vol24h_usd >= 500000) return 1.0;
    
    return std::min(1.0, vol24h_usd / 500000.0);
}

double SignalCalculator::compute_S3(double fdv_liq_ratio) {
    // Preferred: 5-50
    // Penalties: >150 or <2
    if (fdv_liq_ratio >= 5.0 && fdv_liq_ratio <= 50.0) return 1.0;
    if (fdv_liq_ratio > 150.0) return 0.3;
    if (fdv_liq_ratio < 2.0) return 0.4;
    
    // Smooth penalty outside preferred range
    if (fdv_liq_ratio < 5.0) {
        return 0.4 + (fdv_liq_ratio / 5.0) * 0.6;
    }
    if (fdv_liq_ratio > 50.0) {
        double excess = std::min(100.0, fdv_liq_ratio - 50.0);
        return 1.0 - (excess / 100.0) * 0.7;
    }
    
    return 0.8;
}

double SignalCalculator::compute_S4(double m1h, double m24h) {
    // Optimal: m1h +1..+12%, m24h +2..+60%
    // If m1h > +12%: requires entry confirmation
    
    double score = 0.5; // Base
    
    // m1h scoring
    if (m1h >= 1.0 && m1h <= 12.0) {
        score += 0.25;
    } else if (m1h > 12.0) {
        score += 0.1; // Bonus but requires confirmation
    } else if (m1h < 0) {
        score -= 0.2; // Penalty for negative
    }
    
    // m24h scoring
    if (m24h >= 2.0 && m24h <= 60.0) {
        score += 0.25;
    } else if (m24h > 60.0) {
        score += 0.1; // Overheated
    } else if (m24h < 0) {
        score -= 0.2;
    }
    
    return std::max(0.0, std::min(1.0, score));
}

double SignalCalculator::compute_S5(const TokenState& state) {
    // Drawdown structure: bouncing from support
    // Look for higher lows in recent history
    
    if (state.history_24h.size() < 10) return 0.5; // Not enough data
    
    // Simple check: recent low is higher than previous low
    double recent_low = state.latest.price;
    double prev_low = state.latest.price;
    
    for (size_t i = state.history_24h.size() - 20; i < state.history_24h.size() - 10; i++) {
        if (i < state.history_24h.size()) {
            prev_low = std::min(prev_low, state.history_24h[i].price);
        }
    }
    
    for (size_t i = state.history_24h.size() - 10; i < state.history_24h.size(); i++) {
        if (i < state.history_24h.size()) {
            recent_low = std::min(recent_low, state.history_24h[i].price);
        }
    }
    
    if (recent_low > prev_low * 1.02) return 0.9; // Higher low = good structure
    if (recent_low < prev_low * 0.98) return 0.3; // Lower low = weak
    
    return 0.6;
}

double SignalCalculator::compute_S6(const TokenState& state) {
    // Volatility: not too chaotic
    // Check recent price variance
    
    if (state.history_24h.size() < 60) return 0.5;
    
    std::vector<double> recent_prices;
    for (size_t i = state.history_24h.size() - 60; i < state.history_24h.size(); i++) {
        if (i < state.history_24h.size()) {
            recent_prices.push_back(state.history_24h[i].price);
        }
    }
    
    double mean = 0.0;
    for (double p : recent_prices) mean += p;
    mean /= recent_prices.size();
    
    double variance = 0.0;
    for (double p : recent_prices) {
        variance += (p - mean) * (p - mean);
    }
    variance /= recent_prices.size();
    
    double cv = std::sqrt(variance) / mean; // Coefficient of variation
    
    if (cv < 0.05) return 0.9; // Low volatility = good
    if (cv > 0.20) return 0.3; // High volatility = risky
    
    return 0.7;
}

double SignalCalculator::compute_S7(const TokenState& state) {
    // Rug risk heuristics
    // In production: check authorities, holder concentration
    // For now: penalize very young tokens
    
    if (state.latest.age_hours < 24.0) return 0.3; // Very risky
    if (state.latest.age_hours < 72.0) return 0.6; // Young but watchable
    
    return 0.9; // Mature enough
}

double SignalCalculator::compute_S8(double spread_pct, double impact_pct) {
    // Execution cost: spread ≤ 2.5%, impact ≤ 1.5%
    
    if (spread_pct > 2.5) return 0.0; // Hard gate
    if (impact_pct > 1.5) return 0.0; // Hard gate
    
    double score = 1.0;
    score -= (spread_pct / 2.5) * 0.3;
    score -= (impact_pct / 1.5) * 0.3;
    
    return std::max(0.0, score);
}

double SignalCalculator::compute_S9(const TokenState& state) {
    // Volume trend: increasing volume = bullish
    
    if (state.history_24h.size() < 100) return 0.5;
    
    double recent_vol = 0.0;
    double old_vol = 0.0;
    
    for (size_t i = state.history_24h.size() - 50; i < state.history_24h.size(); i++) {
        if (i < state.history_24h.size()) {
            recent_vol += state.history_24h[i].bar_5m.v_usd;
        }
    }
    
    for (size_t i = state.history_24h.size() - 100; i < state.history_24h.size() - 50; i++) {
        if (i < state.history_24h.size()) {
            old_vol += state.history_24h[i].bar_5m.v_usd;
        }
    }
    
    if (recent_vol > old_vol * 1.2) return 0.9; // Volume increasing
    if (recent_vol < old_vol * 0.8) return 0.4; // Volume decreasing
    
    return 0.6;
}

double SignalCalculator::compute_S10(const MarketData::Route& route) {
    // Route health: ≤3 hops, deviation <0.8%
    
    if (!route.ok) return 0.0;
    if (route.hops > 3) return 0.0;
    if (route.dev_pct > 0.8) return 0.3;
    
    double score = 1.0;
    score -= (route.hops - 1) * 0.15; // Penalty per hop
    score -= route.dev_pct * 0.3;
    
    return std::max(0.0, std::min(1.0, score));
}

double SignalCalculator::compute_N1(const std::string& symbol) {
    // Token list hygiene: widely mirrored lists
    // Simplified: penalize if not in known list
    
    // In production: check against CoinGecko, Jupiter strict list, etc.
    static const std::vector<std::string> known_tokens = {
        "SOL", "USDC", "USDT", "BONK", "JUP", "WIF", "JTO"
    };
    
    for (const auto& known : known_tokens) {
        if (symbol == known) return 1.0;
    }
    
    return 0.9; // Small penalty (-10 in C calculation)
}
