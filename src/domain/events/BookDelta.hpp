#pragma once

#include "domain/events/OrderBookEvent.hpp"
#include "domain/events/PriceLevelDelta.hpp"

#include <vector>

namespace mde::domain {

struct BookDelta : OrderBookEvent {
    std::vector<PriceLevelDelta> changes;
};

} // namespace mde::domain
