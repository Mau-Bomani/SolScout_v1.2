#pragma once
#include "../http_client.hpp"
#include <memory>
#include <vector>

struct PoolData {
    std::string address;
    std::string mint_base;
    std::string mint_quote;
    double price;
    double liq_usd;
    double vol24h_usd;
};

class RaydiumClient {
public:
    explicit RaydiumClient(const std::string& base_url, std::shared_ptr<HttpClient> http);
    std::vector<PoolData> get_pools();
private:
    std::string base_url_;
    std::shared_ptr<HttpClient> http_;
};