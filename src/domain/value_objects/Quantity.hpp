#pragma once

#include <compare>
#include <stdexcept>
#include <string>

namespace mde::domain {

class Quantity {
public:
    explicit Quantity(double size);

    static Quantity from_string(const std::string& str);
    static Quantity zero();

    double size() const noexcept { return size_; }

    bool operator==(const Quantity&) const = default;
    auto operator<=>(const Quantity&) const = default;

private:
    double size_;
};

} // namespace mde::domain
