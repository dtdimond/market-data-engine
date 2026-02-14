#pragma once

#include "domain/events/OrderBookEvent.hpp"
#include "domain/value_objects/Price.hpp"

namespace mde::domain {

struct TickSizeChange : OrderBookEvent {
    Price old_tick_size;
    Price new_tick_size;
};

} // namespace mde::domain
