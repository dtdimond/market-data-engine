#include "domain/value_objects/Timestamp.hpp"

#include <stdexcept>

namespace mde::domain {

Timestamp::Timestamp(int64_t milliseconds_since_epoch) : ms_(milliseconds_since_epoch) {
    if (milliseconds_since_epoch < 0) {
        throw std::out_of_range(
            "Timestamp must be non-negative, got: " + std::to_string(milliseconds_since_epoch));
    }
}

Timestamp Timestamp::from_string(const std::string& str) {
    return Timestamp(std::stoll(str));
}

} // namespace mde::domain
