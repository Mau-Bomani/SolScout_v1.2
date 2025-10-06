#include "cg_client.hpp"
#include <spdlog/spdlog.h>

CoinGeckoClient::CoinGeckoClient(const std::string& base_url, int timeout_ms)
    : base_url_(base_url)
    , timeout_ms_(timeout_ms)
    , curl_(curl_easy_init())
{
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL for CoinGecko");
    }
    
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_ms_);
}

CoinGeckoClient::~CoinGeckoClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

size_t CoinGeckoClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

nlohmann::json CoinGeckoClient::make_request(const std::string& endpoint) {
    std::string response_string;
    std::string url = base_url_ + endpoint;
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);
    
    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        spdlog::error("CoinGecko request failed: {}", curl_easy_strerror(res));
        return nlohmann::json{};
    }
    
    try {
        return nlohmann::json::parse(response_string);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse CoinGecko response: {}", e.what());
        return nlohmann::json{};
    }
}

std::optional<double> CoinGeckoClient::get_price_usd(const std::string& symbol) {
    // Convert symbol to CoinGecko ID (simplified - would need mapping table in production)
    std::string cg_id = symbol;
    std::transform(cg_id.begin(), cg_id.end(), cg_id.begin(), ::tolower);
    
    std::string endpoint = "/simple/price?ids=" + cg_id + "&vs_currencies=usd";
    auto response = make_request(endpoint);
    
    if (response.empty() || !response.contains(cg_id)) {
        return std::nullopt;
    }
    
    if (response[cg_id].contains("usd")) {
        return response[cg_id]["usd"].get<double>();
    }
    
    return std::nullopt;
}

bool CoinGeckoClient::is_listed(const std::string& symbol) {
    return get_price_usd(symbol).has_value();
}