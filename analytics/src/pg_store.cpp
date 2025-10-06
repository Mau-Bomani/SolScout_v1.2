#include "pg_store.hpp"
#include <spdlog/spdlog.h>

PostgresStore::PostgresStore(const std::string& dsn) : dsn_(dsn) {}

pqxx::connection PostgresStore::make_connection() {
    return pqxx::connection(dsn_);
}

void PostgresStore::init_schema() {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        
        // Analytics only needs to read from existing tables
        // Schema is created by ingestor and portfolio services
        txn.exec("SELECT 1");
        txn.commit();
        
        spdlog::info("Database connection verified");
    } catch (const std::exception& e) {
        spdlog::error("Failed to verify DB: {}", e.what());
        throw;
    }
}

bool PostgresStore::ping() {
    try {
        auto conn = make_connection();
        pqxx::work txn(conn);
        txn.exec("SELECT 1");
        txn.commit();
        return true;
    } catch (...) {
        return false;
    }
}
