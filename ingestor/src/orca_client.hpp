#pragma once
#include "../http_client.hpp"
#include "raydium_client.hpp" // Reuse PoolData
#include <memory>
#include <vector>

class OrcaClient {
public:
    explicit OrcaClient(const std::string& base_url, std::shared_ptr<HttpClient> http);
    std::vector<PoolData> get_pools();
private:
    std::string base_url_;
    std::shared_ptr<HttpClient> http_;
};