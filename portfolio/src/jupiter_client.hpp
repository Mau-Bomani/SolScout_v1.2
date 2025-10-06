#pragma once

#include <string>
#include <optional>

struct RouteInfo {
    int hops;
    double deviation_pct;
    bool ok;
};

class JupiterClient {
public:
    explicit JupiterClient(const std::string& base_url, int timeout_ms = 8000);
    ~JupiterClient();
    
    std::optional<double> get_price_via_quote(const std::string& from_mint, const std::string& to_mint);
    RouteInfo get_route_to_usdc(const std::string& mint);
    
private:
    std::string base_url_;
    int timeout_ms_;
    void* curl_; // CURL*
};

// dex_client.cpp (inline for brevity)
#include "dex_client.hpp"
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

static size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

DEXClient::DEXClient(const std::string& raydium_base, const std::string& orca_base, int timeout_ms)
    : raydium_base_(raydium_base)
    , orca_base_(orca_base)
    , timeout_ms_(timeout_ms)
    , curl_(curl_easy_init())
{
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL for DEX");
    }
    
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_ms_);
}

DEXClient::~DEXClient() {
    if (curl_) {
        curl_easy_cleanup((CURL*)curl_);
    }
}

std::optional<PoolInfo> DEXClient::get_pool_info(const std::string& mint) {
    // Try Raydium first, then Orca
    auto info = try_raydium(mint);
    if (!info) {
        info = try_orca(mint);
    }
    return info;
}

std::optional<PoolInfo> DEXClient::try_raydium(const std::string& mint) {
    // Simplified - in production would query actual Raydium API
    // For now, return nullopt (not found)
    return std::nullopt;
}

std::optional<PoolInfo> DEXClient::try_orca(const std::string& mint) {
    // Simplified - in production would query actual Orca API
    return std::nullopt;
}

// jupiter_client.cpp (inline)
#include "jupiter_client.hpp"
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

JupiterClient::JupiterClient(const std::string& base_url, int timeout_ms)
    : base_url_(base_url)
    , timeout_ms_(timeout_ms)
    , curl_(curl_easy_init())
{
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL for Jupiter");
    }
    
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_ms_);
}

JupiterClient::~JupiterClient() {
    if (curl_) {
        curl_easy_cleanup((CURL*)curl_);
    }
}

std::optional<double> JupiterClient::get_price_via_quote(const std::string& from_mint, const std::string& to_mint) {
    // Simplified implementation
    // In production: GET /quote?inputMint=X&outputMint=Y&amount=1000000
    return std::nullopt;
}

RouteInfo JupiterClient::get_route_to_usdc(const std::string& mint) {
    // Simplified - would query actual Jupiter routing
    RouteInfo info;
    info.ok = true;
    info.hops = 2;
    info.deviation_pct = 0.3;
    return info;
}