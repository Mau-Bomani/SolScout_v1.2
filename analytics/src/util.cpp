#include "util.hpp"
#include <sstream>
#include <iomanip>

namespace util {

std::string current_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto itt = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&itt), "%FT%TZ");
    return ss.str();
}

int64_t current_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string hash_reasons(const std::vector<std::string>& reasons) {
    // Simple hash: concatenate and use std::hash
    std::string combined;
    for (const auto& r : reasons) {
        combined += r + "|";
    }
    
    std::hash<std::string> hasher;
    size_t hash_val = hasher(combined);
    
    std::ostringstream ss;
    ss << std::hex << hash_val;
    return ss.str();
}

} // namespace util