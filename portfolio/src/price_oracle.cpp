#include "price_oracle.hpp"
#include <spdlog/spdlog.h>

PriceOracle::PriceOracle(std::shared_ptr<CoinGeckoClient> cg,
                         std::shared_ptr<DEXClient> dex,
                         std::shared_ptr<JupiterClient> jupiter)
    : cg_(cg)
    , dex_(dex)
    , jupiter_(jupiter)
{}

void PriceOracle::price_holding(Holding& holding) {
    // Priority 1: CoinGecko
    auto cg_price = cg_->get_price_usd(holding.symbol);
    if (cg_price.has_value()) {
        holding.usd_price = *cg_price;
        holding.usd_value = holding.amount * (*cg_price);
        holding.tag = ValuationTag::CG;
        spdlog::debug("Priced {} via CoinGecko: ${:.2f}", holding.symbol, *cg_price);
        return;
    }
    
    // Priority 2: DEX with >= 75k liquidity
    auto pool_info = dex_->get_pool_info(holding.mint);
    if (pool_info.has_value()) {
        if (pool_info->liquidity_usd >= 75000.0) {
            holding.usd_price = pool_info->price;
            holding.usd_value = holding.amount * pool_info->price;
            holding.tag = ValuationTag::DEX;
            spdlog::debug("Priced {} via DEX: ${:.2f} (liq: ${:.0f})", 
                         holding.symbol, pool_info->price, pool_info->liquidity_usd);
            return;
        }
        
        // Priority 3: DEX with 25k-75k liquidity (50% haircut)
        if (pool_info->liquidity_usd >= 25000.0) {
            holding.usd_price = pool_info->price;
            holding.usd_value = holding.amount * pool_info->price;
            holding.tag = ValuationTag::EST_50;
            spdlog::debug("Priced {} via DEX (haircut): ${:.2f} (liq: ${:.0f})", 
                         holding.symbol, pool_info->price, pool_info->liquidity_usd);
            return;
        }
    }
    
    // Could not price
    holding.tag = ValuationTag::NA;
    spdlog::debug("Could not price {}", holding.symbol);
}