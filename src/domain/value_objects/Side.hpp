#pragma once

#include <stdexcept>
#include <string>

namespace mde::domain {

enum class Side { BUY, SELL };

inline Side side_from_string(const std::string& str) {
    if (str == "BUY") return Side::BUY;
    if (str == "SELL") return Side::SELL;
    throw std::invalid_argument("Invalid side: " + str);
}

} // namespace mde::domain
