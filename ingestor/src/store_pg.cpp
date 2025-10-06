#include "store_pg.hpp"
#include <spdlog/spdlog.h>

PostgresStore::PostgresStore(const std::string& dsn) : dsn_(dsn) {}

pqxx::connection PostgresStore::make_connection() {
    return pqxx::connection(dsn_);
}

void PostgresStore::init_schema() {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS pools (
                id BIGSERIAL PRIMARY KEY,
                address TEXT UNIQUE NOT NULL,
                mint_base TEXT NOT NULL,
                mint_quote TEXT NOT NULL,
                dex TEXT NOT NULL,
                first_seen TIMESTAMPTZ NOT NULL DEFAULT NOW()
            )
        )");
        
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS pool_stats_5m (
                pool_id BIGINT NOT NULL REFERENCES pools(id),
                ts TIMESTAMPTZ NOT NULL,
                price NUMERIC,
                liq_usd NUMERIC,
                vol24h_usd NUMERIC,
                spread_pct NUMERIC,
                impact_1pct_pct NUMERIC,
                route_ok BOOLEAN,
                route_hops INT,
                route_dev_pct NUMERIC,
                dq TEXT,
                PRIMARY KEY (pool_id, ts)
            )
        )");
        
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS pool_stats_15m (
                pool_id BIGINT NOT NULL REFERENCES pools(id),
                ts TIMESTAMPTZ NOT NULL,
                o NUMERIC, h NUMERIC, l NUMERIC, c NUMERIC,
                v_usd NUMERIC,
                PRIMARY KEY (pool_id, ts)
            )
        )");
        
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS token_first_liq (
                mint TEXT PRIMARY KEY,
                first_liq_ts TIMESTAMPTZ NOT NULL,
                first_pool_id BIGINT REFERENCES pools(id)
            )
        )");
        
        txn.commit();
        spdlog::info("Database schema initialized");
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize schema: {}", e.what());
        throw;
    }
}

int64_t PostgresStore::upsert_pool(const NormalizedPool& pool) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        auto result = txn.exec_params(
            "INSERT INTO pools (address, mint_base, mint_quote, dex) "
            "VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (address) DO UPDATE SET dex = $4 "
            "RETURNING id",
            pool.address, pool.mint_base, pool.mint_quote, pool.dex
        );
        
        txn.commit();
        return result[0][0].as<int64_t>();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to upsert pool: {}", e.what());
        throw;
    }
}

void PostgresStore::save_5m_stats(int64_t pool_id, const OHLCVBar& bar,
                                  double liq_usd, double vol24h_usd,
                                  double spread_pct, double impact_pct,
                                  bool route_ok, int route_hops, double route_dev_pct,
                                  const std::string& dq) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        // Convert timestamp to postgres format
        auto ts_sec = bar.timestamp_ms / 1000;
        auto ts_str = fmt::format("to_timestamp({})", ts_sec);
        
        txn.exec_params(
            "INSERT INTO pool_stats_5m "
            "(pool_id, ts, price, liq_usd, vol24h_usd, spread_pct, impact_1pct_pct, "
            "route_ok, route_hops, route_dev_pct, dq) "
            "VALUES ($1, " + ts_str + ", $2, $3, $4, $5, $6, $7, $8, $9, $10) "
            "ON CONFLICT (pool_id, ts) DO UPDATE SET "
            "price = $2, liq_usd = $3, vol24h_usd = $4",
            pool_id, bar.close, liq_usd, vol24h_usd, spread_pct, impact_pct,
            route_ok, route_hops, route_dev_pct, dq
        );
        
        txn.commit();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to save 5m stats: {}", e.what());
    }
}

void PostgresStore::save_15m_bar(int64_t pool_id, const OHLCVBar& bar) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        auto ts_sec = bar.timestamp_ms / 1000;
        auto ts_str = fmt::format("to_timestamp({})", ts_sec);
        
        txn.exec_params(
            "INSERT INTO pool_stats_15m (pool_id, ts, o, h, l, c, v_usd) "
            "VALUES ($1, " + ts_str + ", $2, $3, $4, $5, $6) "
            "ON CONFLICT (pool_id, ts) DO UPDATE SET "
            "o = $2, h = $3, l = $4, c = $5, v_usd = $6",
            pool_id, bar.open, bar.high, bar.low, bar.close, bar.volume_usd
        );
        
        txn.commit();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to save 15m bar: {}", e.what());
    }
}

void PostgresStore::update_token_first_liq(const std::string& mint, double liq_usd, 
                                           int64_t pool_id) {
    if (liq_usd < 25000.0) return; // Only track when crosses 25k threshold
    
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        txn.exec_params(
            "INSERT INTO token_first_liq (mint, first_liq_ts, first_pool_id) "
            "VALUES ($1, NOW(), $2) "
            "ON CONFLICT (mint) DO NOTHING",
            mint, pool_id
        );
        
        txn.commit();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to update first liq: {}", e.what());
    }
}

bool PostgresStore::ping() {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        txn.exec("SELECT 1");
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}