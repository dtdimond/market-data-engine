#pragma once

#include "domain/events/OrderBookEvent.hpp"
#include "domain/value_objects/Price.hpp"
#include "domain/value_objects/Quantity.hpp"
#include "domain/value_objects/Side.hpp"

#include <string>

namespace mde::domain {

struct TradeEvent : OrderBookEvent {
    Price price;
    Quantity size;
    Side side;
    std::string fee_rate_bps;
};

} // namespace mde::domain
