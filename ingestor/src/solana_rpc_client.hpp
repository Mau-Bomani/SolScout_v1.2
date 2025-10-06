#pragma once
#include "../http_client.hpp"
#include <memory>
#include <vector>
#include <string>

class SolanaRPCClient {
public:
    SolanaRPCClient(const std::vector<std::string>& rpc_urls, std::shared_ptr<HttpClient> http);
    bool is_healthy();
private:
    std::vector<std::string> rpc_urls_;
    std::shared_ptr<HttpClient> http_;
    size_t current_index_;
    
    void rotate();
};