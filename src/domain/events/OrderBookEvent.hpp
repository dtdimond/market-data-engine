#pragma once

#include "domain/value_objects/MarketAsset.hpp"
#include "domain/value_objects/Timestamp.hpp"

#include <cstdint>

namespace mde::domain {

struct OrderBookEvent {
    MarketAsset asset;
    Timestamp timestamp;
    uint64_t sequence_number;
};

} // namespace mde::domain
