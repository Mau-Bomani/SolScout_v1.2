#pragma once

#include <string>
#include <chrono>
#include <random>

namespace util {
    std::string generate_uuid();
    std::string current_iso8601();
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delim);
    std::string generate_pin(int length = 6);
}