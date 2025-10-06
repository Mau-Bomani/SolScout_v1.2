#pragma once
#include "../http_client.hpp"
#include <memory>
#include <optional>

struct RouteInfo {
    bool ok;
    int hops;
    double dev_pct;
};

class JupiterClient {
public:
    explicit JupiterClient(const std::string& base_url, std::shared_ptr<HttpClient> http);
    RouteInfo get_route(const std::string& from_mint, const std::string& to_mint);
private:
    std::string base_url_;
    std::shared_ptr<HttpClient> http_;
};
