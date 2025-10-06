#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

class CoinGeckoClient {
public:
    explicit CoinGeckoClient(const std::string& base_url, int timeout_ms = 8000);
    ~CoinGeckoClient();
    
    std::optional<double> get_price_usd(const std::string& symbol);
    bool is_listed(const std::string& symbol);
    
private:
    std::string base_url_;
    int timeout_ms_;
    CURL* curl_;
    
    nlohmann::json make_request(const std::string& endpoint);
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};