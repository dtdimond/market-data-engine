#include "domain/value_objects/Price.hpp"

namespace mde::domain {

Price::Price(double value) : value_(value) {
    if (value < 0.0 || value > 1.0) {
        throw std::out_of_range(
            "Price must be between 0 and 1, got: " + std::to_string(value));
    }
}

Price Price::from_string(const std::string& str) {
    return Price(std::stod(str));
}

Price Price::zero() {
    return Price(0.0);
}

} // namespace mde::domain
