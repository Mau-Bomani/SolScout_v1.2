#pragma once

#include <string>
#include <optional>

struct PoolInfo {
    double price;
    double liquidity_usd;
    std::string source; // "raydium" or "orca"
};

class DEXClient {
public:
    DEXClient(const std::string& raydium_base, const std::string& orca_base, int timeout_ms = 8000);
    ~DEXClient();
    
    std::optional<PoolInfo> get_pool_info(const std::string& mint);
    
private:
    std::string raydium_base_;
    std::string orca_base_;
    int timeout_ms_;
    void* curl_; // CURL*
    
    std::optional<PoolInfo> try_raydium(const std::string& mint);
    std::optional<PoolInfo> try_orca(const std::string& mint);
};
