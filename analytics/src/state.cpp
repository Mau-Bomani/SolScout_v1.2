#include "state.hpp"
#include "util.hpp"
#include <algorithm>

void TokenState::update(const MarketData& md) {
    latest = md;
    history_24h.push_back(md);
    
    // Keep only 24h of data (assuming updates every 60s = max 1440 entries)
    while (history_24h.size() > 1440) {
        history_24h.pop_front();
    }
}

MarketData TokenState::get_1h_ago() const {
    // Find entry ~60 minutes ago (60 entries back if 1min updates)
    if (history_24h.size() < 60) return latest;
    return history_24h[history_24h.size() - 60];
}

MarketData TokenState::get_24h_ago() const {
    if (history_24h.empty()) return latest;
    return history_24h.front();
}

double TokenState::compute_m1h() const {
    auto old_data = get_1h_ago();
    if (old_data.price <= 0 || latest.price <= 0) return 0.0;
    return ((latest.price - old_data.price) / old_data.price) * 100.0;
}

double TokenState::compute_m24h() const {
    auto old_data = get_24h_ago();
    if (old_data.price <= 0 || latest.price <= 0) return 0.0;
    return ((latest.price - old_data.price) / old_data.price) * 100.0;
}

void StateManager::update_token(const std::string& symbol, const MarketData& md) {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_[symbol].update(md);
    tokens_[symbol].symbol = symbol;
}

TokenState* StateManager::get_token(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tokens_.find(symbol);
    if (it == tokens_.end()) return nullptr;
    return &it->second;
}

std::vector<std::string> StateManager::get_all_symbols() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> symbols;
    for (const auto& [sym, _] : tokens_) {
        symbols.push_back(sym);
    }
    return symbols;
}

void StateManager::cleanup_stale(int max_age_hours) {
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t cutoff_ms = util::current_timestamp_ms() - (max_age_hours * 3600 * 1000);
    
    for (auto it = tokens_.begin(); it != tokens_.end();) {
        if (it->second.latest.ts_ms < cutoff_ms) {
            it = tokens_.erase(it);
        } else {
            ++it;
        }
    }
}