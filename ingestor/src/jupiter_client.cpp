#include "jupiter_client.hpp"
#include <spdlog/spdlog.h>

JupiterClient::JupiterClient(const std::string& base_url, std::shared_ptr<HttpClient> http)
    : base_url_(base_url), http_(http) {}

RouteInfo JupiterClient::get_route(const std::string& from_mint, const std::string& to_mint) {
    RouteInfo info;
    info.ok = true;
    info.hops = 2;
    info.dev_pct = 0.3;
    
    // Simplified - would query Jupiter routing API
    std::string url = base_url_ + "/quote?inputMint=" + from_mint + 
                      "&outputMint=" + to_mint + "&amount=1000000";
    
    auto response = http_->get_json(url);
    
    if (!response.has_value()) {
        info.ok = false;
        return info;
    }
    
    // Parse route information from response
    // Implementation depends on Jupiter API format
    
    return info;
}

