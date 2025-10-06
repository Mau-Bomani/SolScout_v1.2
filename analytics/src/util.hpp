#pragma once

#include <string>
#include <chrono>
#include <functional>

namespace util {
    std::string current_iso8601();
    int64_t current_timestamp_ms();
    std::string hash_reasons(const std::vector<std::string>& reasons);
}