#pragma once

#include <vector>
#include <chrono>

struct PriceTick {
    double price;
    double volume_usd;
    int64_t timestamp_ms;
};

struct OHLCVBar {
    double open;
    double high;
    double low;
    double close;
    double volume_usd;
    int64_t timestamp_ms;
    bool degraded; // true if reconstructed from sparse data
};

class BarSynthesizer {
public:
    BarSynthesizer(int interval_seconds);
    
    void add_tick(const PriceTick& tick);
    std::vector<OHLCVBar> get_completed_bars();
    OHLCVBar get_current_bar() const;
    
private:
    int interval_seconds_;
    std::vector<PriceTick> ticks_;
    int64_t current_bar_start_ms_;
    
    OHLCVBar synthesize_bar(int64_t start_ms, const std::vector<PriceTick>& bar_ticks) const;
    bool is_bar_complete(int64_t start_ms) const;
};