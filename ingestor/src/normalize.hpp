#pragma once

#include <string>
#include <nlohmann/json.hpp>

struct NormalizedPool {
    int64_t pool_id;
    std::string address;
    std::string mint_base;
    std::string mint_quote;
    std::string dex;
    double price;
    double liq_usd;
    double vol24h_usd;
    double spread_pct;
    double impact_1pct_pct;
    std::string dq; // "ok" or "degraded"
};

class Normalizer {
public:
    static NormalizedPool normalize_pool(const nlohmann::json& raw_data, 
                                        const std::string& dex_source);
};
