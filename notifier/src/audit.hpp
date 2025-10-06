#pragma once
#include <string>
#include <nlohmann/json.hpp>

class Auditor {
public:
    static nlohmann::json build_audit_event(const std::string& kind,
                                            const std::string& band,
                                            const std::string& symbol,
                                            const std::string& reason);
};