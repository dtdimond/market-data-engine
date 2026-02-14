#pragma once

#include "domain/value_objects/Price.hpp"
#include "domain/value_objects/Quantity.hpp"

namespace mde::domain {

class PriceLevel {
public:
    PriceLevel(Price price, Quantity size);

    static PriceLevel from_strings(const std::string& price, const std::string& size);

    const Price& price() const noexcept { return price_; }
    const Quantity& size() const noexcept { return size_; }

    bool operator==(const PriceLevel&) const = default;
    auto operator<=>(const PriceLevel&) const = default;

private:
    Price price_;
    Quantity size_;
};

} // namespace mde::domain
