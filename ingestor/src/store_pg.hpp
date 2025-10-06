#pragma once

#include "normalize.hpp"
#include "bar_synth.hpp"
#include <pqxx/pqxx>
#include <string>
#include <vector>

class PostgresStore {
public:
    explicit PostgresStore(const std::string& dsn);
    
    void init_schema();
    int64_t upsert_pool(const NormalizedPool& pool);
    void save_5m_stats(int64_t pool_id, const OHLCVBar& bar, 
                      double liq_usd, double vol24h_usd,
                      double spread_pct, double impact_pct,
                      bool route_ok, int route_hops, double route_dev_pct,
                      const std::string& dq);
    void save_15m_bar(int64_t pool_id, const OHLCVBar& bar);
    void update_token_first_liq(const std::string& mint, double liq_usd, int64_t pool_id);
    
    bool ping();
    
private:
    std::string dsn_;
    pqxx::connection make_connection();
};