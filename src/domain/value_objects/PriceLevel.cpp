#include "domain/value_objects/PriceLevel.hpp"

namespace mde::domain {

PriceLevel::PriceLevel(Price price, Quantity size)
    : price_(price)
    , size_(size) {}

PriceLevel PriceLevel::from_strings(const std::string& price, const std::string& size) {
    return PriceLevel(Price::from_string(price), Quantity::from_string(size));
}

} // namespace mde::domain
