#pragma once

#include "domain/events/BookDelta.hpp"
#include "domain/events/BookSnapshot.hpp"
#include "domain/events/TickSizeChange.hpp"
#include "domain/events/TradeEvent.hpp"
#include "domain/value_objects/MarketAsset.hpp"
#include "domain/value_objects/Price.hpp"
#include "domain/value_objects/PriceLevel.hpp"
#include "domain/value_objects/Timestamp.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mde::domain {

using OrderBookEventVariant = std::variant<BookSnapshot, BookDelta, TradeEvent, TickSizeChange>;

struct Spread {
    Price best_bid;
    Price best_ask;

    double value() const { return best_ask.value() - best_bid.value(); }
};

class OrderBook {
public:
    // Factory
    static OrderBook empty(MarketAsset asset);

    // Apply events — returns a new OrderBook (immutable)
    OrderBook apply(const BookSnapshot& event) const;
    OrderBook apply(const BookDelta& event) const;
    OrderBook apply(const TradeEvent& event) const;
    OrderBook apply(const TickSizeChange& event) const;
    OrderBook apply(const OrderBookEventVariant& event) const;

    // Queries
    const MarketAsset& get_asset() const noexcept { return asset_; }
    Spread get_spread() const;
    int get_depth() const noexcept;
    Price get_midpoint() const;
    Price get_best_bid() const;
    Price get_best_ask() const;
    std::optional<TradeEvent> get_latest_trade() const noexcept { return latest_trade_; }
    Price get_tick_size() const noexcept { return tick_size_; }
    Timestamp get_timestamp() const noexcept { return timestamp_; }
    uint64_t get_last_sequence_number() const noexcept { return last_sequence_number_; }
    const std::string& get_book_hash() const noexcept { return book_hash_; }
    const std::vector<PriceLevel>& get_bids() const noexcept { return bids_; }
    const std::vector<PriceLevel>& get_asks() const noexcept { return asks_; }

private:
    OrderBook(MarketAsset asset, std::vector<PriceLevel> bids, std::vector<PriceLevel> asks,
              std::optional<TradeEvent> latest_trade, Price tick_size,
              Timestamp timestamp, uint64_t last_sequence_number, std::string book_hash);

    MarketAsset asset_;
    std::vector<PriceLevel> bids_;   // Sorted descending by price
    std::vector<PriceLevel> asks_;   // Sorted ascending by price
    std::optional<TradeEvent> latest_trade_;
    Price tick_size_;
    Timestamp timestamp_;
    uint64_t last_sequence_number_;
    std::string book_hash_;
};

} // namespace mde::domain
