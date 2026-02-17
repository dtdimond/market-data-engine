#pragma once

#include "domain/aggregates/OrderBook.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace mde::repositories {

class IOrderBookRepository {
public:
    // Event storage (source of truth)
    virtual void append_event(const mde::domain::OrderBookEventVariant& event) = 0;
    virtual std::vector<mde::domain::OrderBookEventVariant> get_events_since(
        const mde::domain::MarketAsset& asset, uint64_t sequence_number) const = 0;

    // Snapshot storage (projection for fast reads)
    virtual void store_snapshot(const mde::domain::OrderBook& book) = 0;
    virtual std::optional<mde::domain::OrderBook> get_latest_snapshot(
        const mde::domain::MarketAsset& asset) const = 0;

    virtual ~IOrderBookRepository() = default;
};

} // namespace mde::repositories
