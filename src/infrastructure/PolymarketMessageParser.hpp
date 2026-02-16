#pragma once

#include "domain/aggregates/OrderBook.hpp"

#include <string>
#include <vector>

namespace mde::infrastructure {

class PolymarketMessageParser {
public:
    // Parse a raw WebSocket JSON message into domain events.
    // Returns empty vector for unrecognized message types.
    // Polymarket wraps messages in a JSON array, so one message
    // can produce multiple events (e.g. price_change with multiple assets).
    std::vector<mde::domain::OrderBookEventVariant> parse(const std::string& json_str) const;
};

} // namespace mde::infrastructure
