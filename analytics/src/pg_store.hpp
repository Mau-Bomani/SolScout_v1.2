#pragma once
#include <string>
#include <pqxx/pqxx>

class PostgresStore {
public:
    explicit PostgresStore(const std::string& dsn);
    void init_schema();
    bool ping();
    
private:
    std::string dsn_;
    pqxx::connection make_connection();
};