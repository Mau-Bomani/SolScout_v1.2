#pragma once

#include "state.hpp"
#include "scoring.hpp"

struct EntryConfirmation {
    bool confirmed;
    std::string method; // "retest_hold", "quick_pullback", "none"
    std::string reason;
};

struct NetEdgeCheck {
    bool passes;
    double upside_pct;    // U to 24h swing high
    double downside_pct;  // K = spread + impact + lag
    std::string reason;
};

struct SizingSuggestion {
    double size_usd;
    double size_sol;
    std::string rationale;
    double est_impact_pct;
};

class EntryExitLogic {
public:
    // Entry confirmation: required if m1h > +12%
    static EntryConfirmation check_entry_confirmation(const TokenState& state);
    
    // Net edge: U ≥ 2×K
    static NetEdgeCheck check_net_edge(const TokenState& state);
    
    // Position sizing based on ATR and liquidity
    static SizingSuggestion compute_sizing(const TokenState& state, 
                                           double wallet_sol,
                                           double regime_size_adj);
    
    // Exit plan template
    static std::string build_exit_plan(const TokenState& state);
    
private:
    static bool check_retest_hold(const TokenState& state);
    static bool check_quick_pullback(const TokenState& state);
    static double estimate_24h_swing_high(const TokenState& state);
};