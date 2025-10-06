#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

struct TokenAccount {
    std::string mint;
    std::string owner;
    uint64_t amount;
    uint8_t decimals;
};

class SolanaRPC {
public:
    explicit SolanaRPC(const std::vector<std::string>& rpc_urls, int timeout_ms = 8000);
    ~SolanaRPC();
    
    std::vector<TokenAccount> get_token_accounts(const std::string& wallet_address);
    bool is_healthy();
    
private:
    std::vector<std::string> rpc_urls_;
    int timeout_ms_;
    CURL* curl_;
    size_t current_rpc_index_;
    
    nlohmann::json make_request(const nlohmann::json& payload);
    void rotate_rpc();
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};