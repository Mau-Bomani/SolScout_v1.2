#include "audit.hpp"
#include "util.hpp"

nlohmann::json Auditor::build_audit_event(const std::string& kind,
                                          const std::string& band,
                                          const std::string& symbol,
                                          const std::string& reason) {
    return {
        {"kind", kind},
        {"band", band},
        {"symbol", symbol},
        {"reason_hash", reason},
        {"ts", util::current_iso8601()}
    };
}