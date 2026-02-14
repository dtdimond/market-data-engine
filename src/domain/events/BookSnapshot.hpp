#pragma once

#include "domain/events/OrderBookEvent.hpp"
#include "domain/value_objects/PriceLevel.hpp"

#include <string>
#include <vector>

namespace mde::domain {

struct BookSnapshot : OrderBookEvent {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    std::string hash;
};

} // namespace mde::domain
