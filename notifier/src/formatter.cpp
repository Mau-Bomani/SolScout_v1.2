#include "formatter.hpp"
#include <fmt/format.h>
#include <algorithm>

AlertFormatter::AlertFormatter(const std::string& timezone) : timezone_(timezone) {}

std::string AlertFormatter::build_title(const std::string& band, const std::string& symbol,
                                        double price, int confidence) {
    std::string action = "BUY";
    std::string band_display;
    
    if (band == "heads_up") {
        band_display = "💡 Heads-up";
    } else if (band == "actionable") {
        band_display = "⚡ Actionable";
    } else if (band == "high_conviction") {
        band_display = "🔥 High Conviction";
    } else {
        band_display = band;
    }
    
    return fmt::format("{} {} — {} @ ${:.6f} (C={})",
                      band_display, action, symbol, price, confidence);
}

std::string AlertFormatter::format_timestamp(const std::string& iso8601) {
    // Simplified - in production would convert to user timezone
    return iso8601.substr(0, 19);
}

FormattedAlert AlertFormatter::format_alert(const nlohmann::json& raw_alert) {
    FormattedAlert result;
    
    std::string band = raw_alert.value("severity", "heads_up");
    std::string symbol = raw_alert.value("symbol", "UNKNOWN");
    double price = raw_alert.value("price", 0.0);
    int confidence = raw_alert.value("confidence", 0);
    
    // Build message
    std::string msg = build_title(band, symbol, price, confidence);
    msg += "\n\n";
    
    // Add reason lines (bullets)
    if (raw_alert.contains("lines") && raw_alert["lines"].is_array()) {
        for (const auto& line : raw_alert["lines"]) {
            msg += "• " + line.get<std::string>() + "\n";
        }
    }
    
    msg += "\n";
    
    // Add plan
    if (raw_alert.contains("plan")) {
        msg += "<b>Plan:</b> " + raw_alert["plan"].get<std::string>() + "\n";
    }
    
    // Add SOL path
    if (raw_alert.contains("sol_path")) {
        msg += "<b>Exit to SOL:</b> " + raw_alert["sol_path"].get<std::string>();
        
        if (raw_alert.contains("est_impact_pct")) {
            msg += fmt::format(" (est impact {:.1f}%)", 
                             raw_alert["est_impact_pct"].get<double>());
        }
        msg += "\n";
    }
    
    // Add timestamp
    if (raw_alert.contains("ts")) {
        msg += "\n<i>" + format_timestamp(raw_alert["ts"].get<std::string>()) + "</i>";
    }
    
    result.text = msg;
    result.parts = split_if_needed(msg);
    result.split_required = result.parts.size() > 1;
    
    return result;
}

std::vector<std::string> AlertFormatter::split_if_needed(const std::string& text) {
    std::vector<std::string> parts;
    
    if (text.length() <= TELEGRAM_MAX_LENGTH) {
        parts.push_back(text);
        return parts;
    }
    
    // Simple split at newlines
    size_t pos = 0;
    size_t last_pos = 0;
    std::string current_part;
    
    while (pos < text.length()) {
        size_t newline = text.find('\n', pos);
        if (newline == std::string::npos) newline = text.length();
        
        std::string line = text.substr(pos, newline - pos + 1);
        
        if (current_part.length() + line.length() > TELEGRAM_MAX_LENGTH) {
            if (!current_part.empty()) {
                parts.push_back(current_part);
                current_part = "...(continued)\n\n" + line;
            }
        } else {
            current_part += line;
        }
        
        pos = newline + 1;
    }
    
    if (!current_part.empty()) {
        parts.push_back(current_part);
    }
    
    return parts;
}