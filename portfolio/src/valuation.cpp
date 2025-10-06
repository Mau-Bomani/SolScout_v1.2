#include "valuation.hpp"
#include <algorithm>

std::string Holding::tag_string() const {
    switch (tag) {
        case ValuationTag::CG: return "CG";
        case ValuationTag::DEX: return "DEX";
        case ValuationTag::EST_50: return "EST_50";
        case ValuationTag::NA: return "NA";
        default: return "UNKNOWN";
    }
}

Valuator::Valuator(double dust_min_usd, int haircut_pct)
    : dust_min_usd_(dust_min_usd)
    , haircut_pct_(haircut_pct)
{}

bool Valuator::is_dust(const Holding& holding) const {
    if (!holding.usd_value.has_value()) return false;
    return *holding.usd_value < dust_min_usd_;
}

PortfolioSummary Valuator::value_portfolio(const std::vector<Holding>& holdings) {
    PortfolioSummary summary;
    summary.total_usd = 0.0;
    summary.included_count = 0;
    summary.excluded_count = 0;
    summary.haircut_subtotal_usd = 0.0;
    
    for (const auto& holding : holdings) {
        // Skip dust
        if (is_dust(holding)) {
            continue;
        }
        
        // Process by tag
        if (holding.tag == ValuationTag::CG || holding.tag == ValuationTag::DEX) {
            // Include in main total
            if (holding.usd_value.has_value()) {
                summary.total_usd += *holding.usd_value;
                summary.included_count++;
                summary.holdings.push_back(holding);
            }
        } else if (holding.tag == ValuationTag::EST_50) {
            // Haircut subtotal (not in main total)
            if (holding.usd_value.has_value()) {
                double haircutted = *holding.usd_value * (haircut_pct_ / 100.0);
                summary.haircut_subtotal_usd += haircutted;
                
                // Store haircutted value
                Holding h = holding;
                h.usd_value = haircutted;
                summary.holdings.push_back(h);
            }
        } else {
            // NA - not valueable
            summary.excluded_count++;
        }
    }
    
    // Sort holdings by USD value descending
    std::sort(summary.holdings.begin(), summary.holdings.end(),
              [](const Holding& a, const Holding& b) {
                  if (!a.usd_value.has_value()) return false;
                  if (!b.usd_value.has_value()) return true;
                  return *a.usd_value > *b.usd_value;
              });
    
    // Build notes
    if (summary.excluded_count > 0) {
        summary.notes = "Excludes " + std::to_string(summary.excluded_count) + " unpriced tokens.";
    }
    if (summary.haircut_subtotal_usd > 0.0) {
        if (!summary.notes.empty()) summary.notes += " ";
        summary.notes += "Haircut subtotal: $" + std::to_string(summary.haircut_subtotal_usd);
    }
    
    return summary;
}