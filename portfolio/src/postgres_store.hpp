#pragma once

#include "valuation.hpp"
#include <string>
#include <vector>
#include <pqxx/pqxx>
#include <memory>

class PostgresStore {
public:
    explicit PostgresStore(const std::string& dsn);
    
    void init_schema();
    
    int64_t create_or_get_user(int64_t tg_user_id, const std::string& role);
    int64_t add_wallet(int64_t user_id, const std::string& address);
    void remove_wallet(const std::string& address);
    std::vector<std::string> get_active_wallets(int64_t user_id);
    
    int64_t save_snapshot(int64_t wallet_id, const PortfolioSummary& summary);
    void save_holdings(int64_t snapshot_id, const std::vector<Holding>& holdings);
    
    void save_token_metadata(const std::string& mint, const std::string& symbol,
                            const std::string& name, int decimals);
    
    bool ping();
    
private:
    std::string dsn_;
    
    pqxx::connection make_connection();
};