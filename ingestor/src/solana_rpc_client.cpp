#include "solana_rpc_client.hpp"
#include <spdlog/spdlog.h>

SolanaRPCClient::SolanaRPCClient(const std::vector<std::string>& rpc_urls, 
                                 std::shared_ptr<HttpClient> http)
    : rpc_urls_(rpc_urls), http_(http), current_index_(0) {}

void SolanaRPCClient::rotate() {
    current_index_ = (current_index_ + 1) % rpc_urls_.size();
    spdlog::info("Rotated to RPC: {}", rpc_urls_[current_index_]);
}

bool SolanaRPCClient::is_healthy() {
    // Simple health check
    return !rpc_urls_.empty();
}