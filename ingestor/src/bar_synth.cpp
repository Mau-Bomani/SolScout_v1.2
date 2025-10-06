#include "bar_synth.hpp"
#include "util.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

BarSynthesizer::BarSynthesizer(int interval_seconds)
    : interval_seconds_(interval_seconds)
    , current_bar_start_ms_(0)
{}

void BarSynthesizer::add_tick(const PriceTick& tick) {
    if (current_bar_start_ms_ == 0) {
        // Initialize to start of current interval
        current_bar_start_ms_ = (tick.timestamp_ms / (interval_seconds_ * 1000)) * 
                                (interval_seconds_ * 1000);
    }
    
    ticks_.push_back(tick);
}

bool BarSynthesizer::is_bar_complete(int64_t start_ms) const {
    int64_t bar_end_ms = start_ms + (interval_seconds_ * 1000);
    int64_t now_ms = util::current_timestamp_ms();
    return now_ms >= bar_end_ms;
}

std::vector<OHLCVBar> BarSynthesizer::get_completed_bars() {
    std::vector<OHLCVBar> completed;
    
    if (ticks_.empty()) return completed;
    
    // Group ticks by bar interval
    std::sort(ticks_.begin(), ticks_.end(), 
              [](const PriceTick& a, const PriceTick& b) {
                  return a.timestamp_ms < b.timestamp_ms;
              });
    
    int64_t bar_start = current_bar_start_ms_;
    std::vector<PriceTick> bar_ticks;
    
    for (const auto& tick : ticks_) {
        int64_t tick_bar_start = (tick.timestamp_ms / (interval_seconds_ * 1000)) * 
                                 (interval_seconds_ * 1000);
        
        if (tick_bar_start != bar_start) {
            // New bar started
            if (!bar_ticks.empty() && is_bar_complete(bar_start)) {
                completed.push_back(synthesize_bar(bar_start, bar_ticks));
            }
            bar_start = tick_bar_start;
            bar_ticks.clear();
        }
        
        bar_ticks.push_back(tick);
    }
    
    // Keep incomplete bar ticks for next round
    ticks_ = bar_ticks;
    current_bar_start_ms_ = bar_start;
    
    return completed;
}

OHLCVBar BarSynthesizer::get_current_bar() const {
    if (ticks_.empty()) {
        return OHLCVBar{0, 0, 0, 0, 0, 0, true};
    }
    return synthesize_bar(current_bar_start_ms_, ticks_);
}

OHLCVBar BarSynthesizer::synthesize_bar(int64_t start_ms, 
                                         const std::vector<PriceTick>& bar_ticks) const {
    OHLCVBar bar;
    bar.timestamp_ms = start_ms;
    bar.volume_usd = 0.0;
    bar.degraded = bar_ticks.size() < 3; // Mark as degraded if sparse
    
    if (bar_ticks.empty()) {
        bar.open = bar.high = bar.low = bar.close = 0.0;
        bar.degraded = true;
        return bar;
    }
    
    bar.open = bar_ticks.front().price;
    bar.close = bar_ticks.back().price;
    bar.high = bar_ticks.front().price;
    bar.low = bar_ticks.front().price;
    
    for (const auto& tick : bar_ticks) {
        bar.high = std::max(bar.high, tick.price);
        bar.low = std::min(bar.low, tick.price);
        bar.volume_usd += tick.volume_usd;
    }
    
    return bar;
}
