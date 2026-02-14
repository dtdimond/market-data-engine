#pragma once

#include <compare>
#include <stdexcept>
#include <string>

namespace mde::domain {

class Price {
public:
    explicit Price(double value);

    static Price from_string(const std::string& str);
    static Price zero();

    double value() const noexcept { return value_; }

    bool operator==(const Price&) const = default;
    auto operator<=>(const Price&) const = default;

private:
    double value_;
};

} // namespace mde::domain
