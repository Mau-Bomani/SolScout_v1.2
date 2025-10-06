#include "entry_exit.hpp"
#include <algorithm>
#include <cmath>

EntryConfirmation EntryExitLogic::check_entry_confirmation(const TokenState& state) {
    EntryConfirmation result;
    result.confirmed = false;
    result.method = "none";
    
    double m1h = state.compute_m1h();
    
    // Only required if m1h > +12%
    if (m1h <= 12.0) {
        result.confirmed = true;
        result.method = "not_required";
        result.reason = "m1h within normal range";
        return result;
    }
    
    // Check for retest and hold
    if (check_retest_hold(state)) {
        result.confirmed = true;
        result.method = "retest_hold";
        result.reason = "Retest and hold confirmed";
        return result;
    }
    
    // Check for quick pullback
    if (check_quick_pullback(state)) {
        result.confirmed = true;
        result.method = "quick_pullback";
        result.reason = "Quick pullback confirmed";
        return result;
    }
    
    result.reason = "Awaiting entry confirmation (spike cap)";
    return result;
}

bool EntryExitLogic::check_retest_hold(const TokenState& state) {
    // Retest and hold: 5m close back above prior breakout
    
    if (state.history_24h.size() < 20) return false;
    
    // Find recent high
    double recent_high = 0.0;
    for (size_t i = state.history_24h.size() - 20; i < state.history_24h.size() - 5; i++) {
        if (i < state.history_24h.size()) {
            recent_high = std::max(recent_high, state.history_24h[i].price);
        }
    }
    
    // Check if pulled back and then closed above
    double pullback_low = state.latest.price;
    for (size_t i = state.history_24h.size() - 5; i < state.history_24h.size(); i++) {
        if (i < state.history_24h.size()) {
            pullback_low = std::min(pullback_low, state.history_24h[i].price);
        }
    }
    
    // Confirm: pulled back below high, then 5m close back above
    if (pullback_low < recent_high * 0.98 && 
        state.latest.bar_5m.c > recent_high) {
        return true;
    }
    
    return false;
}

bool EntryExitLogic::check_quick_pullback(const TokenState& state) {
    // Quick pullback 2-5%, then 15m VWAP-proxy close above
    
    if (state.history_24h.size() < 30) return false;
    
    double recent_high = 0.0;
    for (size_t i = state.history_24h.size() - 30; i < state.history_24h.size() - 15; i++) {
        if (i < state.history_24h.size()) {
            recent_high = std::max(recent_high, state.history_24h[i].price);
        }
    }
    
    double pullback_low = state.latest.price;
    for (size_t i = state.history_24h.size() - 15; i < state.history_24h.size(); i++) {
        if (i < state.history_24h.size()) {
            pullback_low = std::min(pullback_low, state.history_24h[i].price);
        }
    }
    
    double pullback_pct = ((recent_high - pullback_low) / recent_high) * 100.0;
    
    // Check: 2-5% pullback, then 15m close above VWAP proxy
    if (pullback_pct >= 2.0 && pullback_pct <= 5.0 &&
        state.latest.bar_15m.c > recent_high) {
        return true;
    }
    
    return false;
}

double EntryExitLogic::estimate_24h_swing_high(const TokenState& state) {
    if (state.history_24h.empty()) return state.latest.price * 1.15;
    
    double swing_high = state.latest.price;
    for (const auto& md : state.history_24h) {
        swing_high = std::max(swing_high, md.price);
    }
    
    // Cap at +15% from current
    return std::min(swing_high, state.latest.price * 1.15);
}

NetEdgeCheck EntryExitLogic::check_net_edge(const TokenState& state) {
    NetEdgeCheck result;
    
    // U = upside to 24h swing high (capped at 15%)
    double swing_high = estimate_24h_swing_high(state);
    result.upside_pct = ((swing_high - state.latest.price) / state.latest.price) * 100.0;
    result.upside_pct = std::min(result.upside_pct, 15.0);
    
    // K = spread + impact + lag (0.30%)
    result.downside_pct = state.latest.spread_pct + 
                         state.latest.impact_1pct_pct + 
                         0.30;
    
    // Must have U ≥ 2×K
    result.passes = (result.upside_pct >= 2.0 * result.downside_pct);
    
    if (result.passes) {
        result.reason = "Net edge positive";
    } else {
        result.reason = "Insufficient upside vs execution cost";
    }
    
    return result;
}

SizingSuggestion EntryExitLogic::compute_sizing(const TokenState& state,
                                                double wallet_sol,
                                                double regime_size_adj) {
    SizingSuggestion sug;
    
    // ATR cap: ~0.6% wallet_sol risk per 1× ATR(1h)
    // Simplified: use recent volatility as ATR proxy
    double atr_proxy = state.latest.price * 0.05; // 5% ATR estimate
    double atr_cap_sol = wallet_sol * 0.006 / (atr_proxy / state.latest.price);
    
    // Liquidity cap: Size_USD ≤ 0.008 × LiquidityUSD
    double liq_cap_usd = state.latest.liq_usd * 0.008;
    double sol_price = 100.0; // Placeholder
    double liq_cap_sol = liq_cap_usd / sol_price;
    
    // Take minimum
    sug.size_sol = std::min(atr_cap_sol, liq_cap_sol);
    
    // Apply regime adjustment
    sug.size_sol *= (1.0 + regime_size_adj / 100.0);
    
    // Global caps: ≤35% wallet, keep 5-10% free
    sug.size_sol = std::min(sug.size_sol, wallet_sol * 0.30);
    
    sug.size_usd = sug.size_sol * sol_price;
    sug.est_impact_pct = state.latest.impact_1pct_pct * (sug.size_usd / state.latest.liq_usd) * 100.0;
    
    sug.rationale = "ATR and liquidity capped";
    
    return sug;
}

std::string EntryExitLogic::build_exit_plan(const TokenState& state) {
    return "Trim 25% at +15; 25% at +30; trail rest";
}