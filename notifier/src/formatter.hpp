#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct FormattedAlert {
    std::string text;
    bool split_required;
    std::vector<std::string> parts;
};

class AlertFormatter {
public:
    explicit AlertFormatter(const std::string& timezone = "America/Denver");
    
    FormattedAlert format_alert(const nlohmann::json& raw_alert);
    
private:
    std::string timezone_;
    static constexpr size_t TELEGRAM_MAX_LENGTH = 4000;
    
    std::string build_title(const std::string& band, const std::string& symbol,
                           double price, int confidence);
    std::string format_timestamp(const std::string& iso8601);
    std::vector<std::string> split_if_needed(const std::string& text);
};