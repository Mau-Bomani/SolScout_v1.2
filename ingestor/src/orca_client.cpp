#include "orca_client.hpp"
#include <spdlog/spdlog.h>

OrcaClient::OrcaClient(const std::string& base_url, std::shared_ptr<HttpClient> http)
    : base_url_(base_url), http_(http) {}

std::vector<PoolData> OrcaClient::get_pools() {
    std::vector<PoolData> pools;
    
    auto response = http_->get_json(base_url_ + "/pools");
    
    if (!response.has_value()) {
        spdlog::warn("Failed to fetch Orca pools");
        return pools;
    }
    
    return pools;
}
