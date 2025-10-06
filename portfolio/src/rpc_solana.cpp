#include "rpc_solana.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

SolanaRPC::SolanaRPC(const std::vector<std::string>& rpc_urls, int timeout_ms)
    : rpc_urls_(rpc_urls)
    , timeout_ms_(timeout_ms)
    , curl_(curl_easy_init())
    , current_rpc_index_(0)
{
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_ms_);
}

SolanaRPC::~SolanaRPC() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

size_t SolanaRPC::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void SolanaRPC::rotate_rpc() {
    current_rpc_index_ = (current_rpc_index_ + 1) % rpc_urls_.size();
    spdlog::warn("Rotated to RPC endpoint: {}", rpc_urls_[current_rpc_index_]);
}

nlohmann::json SolanaRPC::make_request(const nlohmann::json& payload) {
    std::string response_string;
    std::string url = rpc_urls_[current_rpc_index_];
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, payload.dump().c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        spdlog::error("RPC request failed: {}", curl_easy_strerror(res));
        rotate_rpc();
        return nlohmann::json{{"error", curl_easy_strerror(res)}};
    }
    
    try {
        return nlohmann::json::parse(response_string);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse RPC response: {}", e.what());
        return nlohmann::json{{"error", "Parse error"}};
    }
}

std::vector<TokenAccount> SolanaRPC::get_token_accounts(const std::string& wallet_address) {
    std::vector<TokenAccount> accounts;
    
    nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "getTokenAccountsByOwner"},
        {"params", {
            wallet_address,
            {{"programId", "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"}},
            {{"encoding", "jsonParsed"}}
        }}
    };
    
    auto response = make_request(payload);
    
    if (response.contains("error")) {
        spdlog::error("RPC error: {}", response["error"].dump());
        return accounts;
    }
    
    if (!response.contains("result") || !response["result"].contains("value")) {
        return accounts;
    }
    
    for (const auto& account_info : response["result"]["value"]) {
        try {
            const auto& parsed = account_info["account"]["data"]["parsed"]["info"];
            
            TokenAccount ta;
            ta.mint = parsed["mint"].get<std::string>();
            ta.owner = parsed["owner"].get<std::string>();
            
            std::string amount_str = parsed["tokenAmount"]["amount"].get<std::string>();
            ta.amount = std::stoull(amount_str);
            ta.decimals = parsed["tokenAmount"]["decimals"].get<uint8_t>();
            
            // Skip zero balances
            if (ta.amount > 0) {
                accounts.push_back(ta);
            }
            
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse token account: {}", e.what());
        }
    }
    
    spdlog::info("Found {} token accounts for wallet {}", accounts.size(), 
                 wallet_address.substr(0, 8) + "...");
    
    return accounts;
}

bool SolanaRPC::is_healthy() {
    nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "getHealth"}
    };
    
    auto response = make_request(payload);
    return response.contains("result") && response["result"] == "ok";
}