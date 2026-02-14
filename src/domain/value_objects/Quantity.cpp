#include "domain/value_objects/Quantity.hpp"

namespace mde::domain {

Quantity::Quantity(double size) : size_(size) {
    if (size < 0.0) {
        throw std::out_of_range(
            "Quantity must be non-negative, got: " + std::to_string(size));
    }
}

Quantity Quantity::from_string(const std::string& str) {
    return Quantity(std::stod(str));
}

Quantity Quantity::zero() {
    return Quantity(0.0);
}

} // namespace mde::domain
