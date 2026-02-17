#pragma once

#include "domain/aggregates/OrderBook.hpp"

#include <functional>
#include <string>

namespace mde::services {

class IMarketDataFeed {
public:
    using EventCallback = std::function<void(const mde::domain::OrderBookEventVariant&)>;

    virtual void set_on_event(EventCallback callback) = 0;
    virtual void subscribe(const std::string& token_id) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual ~IMarketDataFeed() = default;
};

} // namespace mde::services
