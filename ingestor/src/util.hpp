#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <random>

namespace util {
    std::string current_iso8601();
    std::vector<std::string> split(const std::string& str, char delim);
    int64_t current_timestamp_ms();
    int random_jitter(int min_ms, int max_ms);
}