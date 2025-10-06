#include "normalize.hpp"
#include "impact_model.hpp"
#include <spdlog/spdlog.h>

NormalizedPool Normalizer::normalize_pool(const nlohmann::json& raw_data, 
                                          const std::string& dex_source) {
    NormalizedPool pool;
    pool.dex = dex_source;
    pool.dq = "ok";
    
    try {
        // Extract common fields (format varies by DEX)
        pool.address = raw_data.value("address", "unknown");
        pool.mint_base = raw_data.value("mint_base", "");
        pool.mint_quote = raw_data.value("mint_quote", "");
        
        // Price and liquidity
        pool.price = raw_data.value("price", 0.0);
        pool.liq_usd = raw_data.value("liquidity_usd", 0.0);
        pool.vol24h_usd = raw_data.value("volume_24h_usd", 0.0);
        
        // Compute derived metrics
        double reserve_base = raw_data.value("reserve_base", 1000000.0);
        double reserve_quote = raw_data.value("reserve_quote", 1000000.0);
        
        pool.spread_pct = ImpactModel::estimate_spread_pct(reserve_base, reserve_quote);
        pool.impact_1pct_pct = ImpactModel::calculate_1pct_impact(
            reserve_base, reserve_quote, pool.liq_usd);
        
        // Mark as degraded if missing key data
        if (pool.liq_usd == 0.0 || pool.price == 0.0) {
            pool.dq = "degraded";
        }
        
    } catch (const std::exception& e) {
        spdlog::warn("Failed to normalize pool: {}", e.what());
        pool.dq = "degraded";
    }
    
    return pool;
}