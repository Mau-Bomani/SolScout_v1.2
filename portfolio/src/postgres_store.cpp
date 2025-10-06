#include "postgres_store.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>

PostgresStore::PostgresStore(const std::string& dsn) : dsn_(dsn) {
    spdlog::info("PostgresStore initialized: {}", util::redact_dsn(dsn));
}

pqxx::connection PostgresStore::make_connection() {
    return pqxx::connection(dsn_);
}

void PostgresStore::init_schema() {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        // Users table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS users (
                id BIGSERIAL PRIMARY KEY,
                tg_user_id BIGINT UNIQUE NOT NULL,
                role TEXT NOT NULL CHECK (role IN ('owner','guest')),
                created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
            )
        )");
        
        // Wallets table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS wallets (
                id BIGSERIAL PRIMARY KEY,
                address TEXT UNIQUE NOT NULL,
                owner_user_id BIGINT NOT NULL REFERENCES users(id),
                created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                is_active BOOLEAN NOT NULL DEFAULT TRUE
            )
        )");
        
        // Token metadata table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS token_metadata (
                mint TEXT PRIMARY KEY,
                symbol TEXT,
                name TEXT,
                decimals INT,
                flags JSONB,
                updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
            )
        )");
        
        // Price points table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS price_points (
                mint TEXT NOT NULL,
                source TEXT NOT NULL,
                usd NUMERIC,
                tag TEXT,
                ts TIMESTAMPTZ NOT NULL,
                PRIMARY KEY (mint, ts, source)
            )
        )");
        
        // Portfolio snapshots table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS portfolio_snapshots (
                id BIGSERIAL PRIMARY KEY,
                wallet_id BIGINT NOT NULL REFERENCES wallets(id),
                ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                total_usd NUMERIC,
                included_count INT,
                excluded_count INT,
                haircut_subtotal_usd NUMERIC,
                notes TEXT
            )
        )");
        
        // Holding values table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS holding_values (
                snapshot_id BIGINT NOT NULL REFERENCES portfolio_snapshots(id) ON DELETE CASCADE,
                mint TEXT NOT NULL,
                amount NUMERIC NOT NULL,
                usd_price NUMERIC,
                usd_value NUMERIC,
                valuation_tag TEXT NOT NULL,
                PRIMARY KEY (snapshot_id, mint)
            )
        )");
        
        // Audits table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS audits (
                id BIGSERIAL PRIMARY KEY,
                event TEXT NOT NULL,
                actor_tg_id BIGINT,
                detail TEXT,
                ts TIMESTAMPTZ NOT NULL DEFAULT NOW()
            )
        )");
        
        txn.commit();
        spdlog::info("Database schema initialized");
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize schema: {}", e.what());
        throw;
    }
}

int64_t PostgresStore::create_or_get_user(int64_t tg_user_id, const std::string& role) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        auto result = txn.exec_params(
            "INSERT INTO users (tg_user_id, role) VALUES ($1, $2) "
            "ON CONFLICT (tg_user_id) DO UPDATE SET role = $2 "
            "RETURNING id",
            tg_user_id, role
        );
        
        txn.commit();
        return result[0][0].as<int64_t>();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to create/get user: {}", e.what());
        throw;
    }
}

int64_t PostgresStore::add_wallet(int64_t user_id, const std::string& address) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        auto result = txn.exec_params(
            "INSERT INTO wallets (address, owner_user_id, is_active) "
            "VALUES ($1, $2, TRUE) "
            "ON CONFLICT (address) DO UPDATE SET is_active = TRUE "
            "RETURNING id",
            address, user_id
        );
        
        txn.commit();
        return result[0][0].as<int64_t>();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to add wallet: {}", e.what());
        throw;
    }
}

void PostgresStore::remove_wallet(const std::string& address) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        txn.exec_params(
            "UPDATE wallets SET is_active = FALSE WHERE address = $1",
            address
        );
        
        txn.commit();
        spdlog::info("Deactivated wallet {}", address.substr(0, 8) + "...");
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to remove wallet: {}", e.what());
        throw;
    }
}

std::vector<std::string> PostgresStore::get_active_wallets(int64_t user_id) {
    std::vector<std::string> wallets;
    
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        auto result = txn.exec_params(
            "SELECT address FROM wallets WHERE owner_user_id = $1 AND is_active = TRUE",
            user_id
        );
        
        for (const auto& row : result) {
            wallets.push_back(row[0].as<std::string>());
        }
        
        txn.commit();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to get active wallets: {}", e.what());
    }
    
    return wallets;
}

int64_t PostgresStore::save_snapshot(int64_t wallet_id, const PortfolioSummary& summary) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        auto result = txn.exec_params(
            "INSERT INTO portfolio_snapshots "
            "(wallet_id, total_usd, included_count, excluded_count, haircut_subtotal_usd, notes) "
            "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
            wallet_id,
            summary.total_usd,
            summary.included_count,
            summary.excluded_count,
            summary.haircut_subtotal_usd,
            summary.notes
        );
        
        txn.commit();
        return result[0][0].as<int64_t>();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to save snapshot: {}", e.what());
        throw;
    }
}

void PostgresStore::save_holdings(int64_t snapshot_id, const std::vector<Holding>& holdings) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        for (const auto& h : holdings) {
            double price = h.usd_price.value_or(0.0);
            double value = h.usd_value.value_or(0.0);
            
            txn.exec_params(
                "INSERT INTO holding_values "
                "(snapshot_id, mint, amount, usd_price, usd_value, valuation_tag) "
                "VALUES ($1, $2, $3, $4, $5, $6)",
                snapshot_id,
                h.mint,
                h.amount,
                price,
                value,
                h.tag_string()
            );
        }
        
        txn.commit();
        spdlog::info("Saved {} holdings for snapshot {}", holdings.size(), snapshot_id);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to save holdings: {}", e.what());
        throw;
    }
}

void PostgresStore::save_token_metadata(const std::string& mint, const std::string& symbol,
                                       const std::string& name, int decimals) {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        txn.exec_params(
            "INSERT INTO token_metadata (mint, symbol, name, decimals) "
            "VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (mint) DO UPDATE SET "
            "symbol = $2, name = $3, decimals = $4, updated_at = NOW()",
            mint, symbol, name, decimals
        );
        
        txn.commit();
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to save token metadata: {}", e.what());
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
        spdlog::error("Postgres ping failed: {}", e.what());
        return false;
    }
}