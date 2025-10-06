#pragma once

#include <string>
#include <map>
#include <vector>
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>

struct MarketData {
    std::string pool;
    std::string mint_base;
    std::string mint_quote;
    double price;
    double liq_usd;
    double vol24h_usd;
    double spread_pct;
    double impact_1pct_pct;
    double age_hours;
    
    struct Route {
        bool ok;
        int hops;
        double dev_pct;
    } route;
    
    struct Bar {
        double o, h, l, c, v_usd;
    };
    Bar bar_5m;
    Bar bar_15m;
    
    std::string dq;
    int64_t ts_ms;
};

struct TokenState {
    std::string symbol;
    MarketData latest;
    std::deque<MarketData> history_24h;  // Rolling 24h window
    
    // Metadata
    double entry_price;
    bool has_position;
    int64_t first_liq_ts_ms;
    
    void update(const MarketData& md);
    MarketData get_1h_ago() const;
    MarketData get_24h_ago() const;
    double compute_m1h() const;
    double compute_m24h() const;
};

class StateManager {
public:
    void update_token(const std::string& symbol, const MarketData& md);
    TokenState* get_token(const std::string& symbol);
    std::vector<std::string> get_all_symbols();
    
    void cleanup_stale(int max_age_hours);
    
private:
    std::mutex mutex_;
    std::map<std::string, TokenState> tokens_;
};