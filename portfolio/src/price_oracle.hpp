#pragma once

#include "cg_client.hpp"
#include "dex_client.hpp"
#include "jupiter_client.hpp"
#include "valuation.hpp"
#include <memory>

class PriceOracle {
public:
    PriceOracle(std::shared_ptr<CoinGeckoClient> cg,
                std::shared_ptr<DEXClient> dex,
                std::shared_ptr<JupiterClient> jupiter);
    
    void price_holding(Holding& holding);
    
private:
    std::shared_ptr<CoinGeckoClient> cg_;
    std::shared_ptr<DEXClient> dex_;
    std::shared_ptr<JupiterClient> jupiter_;
};
