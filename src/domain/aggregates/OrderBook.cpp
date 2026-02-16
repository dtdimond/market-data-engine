#include "domain/aggregates/OrderBook.hpp"

#include <algorithm>
#include <stdexcept>

namespace mde::domain {

OrderBook::OrderBook(MarketAsset asset, std::vector<PriceLevel> bids, std::vector<PriceLevel> asks,
                     std::optional<TradeEvent> latest_trade, Price tick_size,
                     Timestamp timestamp, uint64_t last_sequence_number, std::string book_hash)
    : asset_(std::move(asset))
    , bids_(std::move(bids))
    , asks_(std::move(asks))
    , latest_trade_(std::move(latest_trade))
    , tick_size_(tick_size)
    , timestamp_(timestamp)
    , last_sequence_number_(last_sequence_number)
    , book_hash_(std::move(book_hash)) {}

OrderBook OrderBook::empty(MarketAsset asset) {
    return OrderBook(std::move(asset), {}, {}, std::nullopt, Price(0.01), Timestamp(0), 0, "");
}

// BookSnapshot: replace entire book state
OrderBook OrderBook::apply(const BookSnapshot& event) const {
    // Bids sorted descending by price
    auto bids = event.bids;
    std::sort(bids.begin(), bids.end(),
              [](const PriceLevel& a, const PriceLevel& b) {
                  return a.price() > b.price();
              });

    // Asks sorted ascending by price
    auto asks = event.asks;
    std::sort(asks.begin(), asks.end(),
              [](const PriceLevel& a, const PriceLevel& b) {
                  return a.price() < b.price();
              });

    return OrderBook(asset_, std::move(bids), std::move(asks),
                     latest_trade_, tick_size_,
                     event.timestamp, event.sequence_number, event.hash);
}

namespace {

// Update a sorted price level vector with a new level.
// For bids: descending order (comp should be >).
// For asks: ascending order (comp should be <).
template <typename Compare>
std::vector<PriceLevel> update_levels(std::vector<PriceLevel> levels,
                                       Price price, Quantity new_size,
                                       Compare comp) {
    // Find existing level at this price
    auto it = std::find_if(levels.begin(), levels.end(),
                           [&](const PriceLevel& lvl) { return lvl.price() == price; });

    if (new_size.size() == 0.0) {
        // Remove level
        if (it != levels.end()) {
            levels.erase(it);
        }
    } else if (it != levels.end()) {
        // Update existing level
        *it = PriceLevel(price, new_size);
    } else {
        // Insert new level in sorted position
        auto pos = std::lower_bound(levels.begin(), levels.end(), price,
                                    [&](const PriceLevel& lvl, const Price& p) {
                                        return comp(lvl.price(), p);
                                    });
        levels.insert(pos, PriceLevel(price, new_size));
    }

    return levels;
}

} // anonymous namespace

// BookDelta: patch individual price levels
OrderBook OrderBook::apply(const BookDelta& event) const {
    auto bids = bids_;
    auto asks = asks_;

    for (const auto& change : event.changes) {
        if (change.side == Side::BUY) {
            bids = update_levels(std::move(bids), change.price, change.new_size,
                                 std::greater<Price>{});
        } else {
            asks = update_levels(std::move(asks), change.price, change.new_size,
                                 std::less<Price>{});
        }
    }

    return OrderBook(asset_, std::move(bids), std::move(asks),
                     latest_trade_, tick_size_,
                     event.timestamp, event.sequence_number, book_hash_);
}

// TradeEvent: record latest trade
OrderBook OrderBook::apply(const TradeEvent& event) const {
    return OrderBook(asset_, bids_, asks_,
                     event, tick_size_,
                     event.timestamp, event.sequence_number, book_hash_);
}

// TickSizeChange: update tick size
OrderBook OrderBook::apply(const TickSizeChange& event) const {
    return OrderBook(asset_, bids_, asks_,
                     latest_trade_, event.new_tick_size,
                     event.timestamp, event.sequence_number, book_hash_);
}

// Variant dispatch
OrderBook OrderBook::apply(const OrderBookEventVariant& event) const {
    return std::visit([this](const auto& e) { return this->apply(e); }, event);
}

Spread OrderBook::get_spread() const {
    return Spread{get_best_bid(), get_best_ask()};
}

int OrderBook::get_depth() const noexcept {
    return static_cast<int>(std::max(bids_.size(), asks_.size()));
}

Price OrderBook::get_midpoint() const {
    auto bid = get_best_bid().value();
    auto ask = get_best_ask().value();
    return Price((bid + ask) / 2.0);
}

Price OrderBook::get_best_bid() const {
    if (bids_.empty()) {
        throw std::runtime_error("No bids in order book");
    }
    return bids_.front().price();
}

Price OrderBook::get_best_ask() const {
    if (asks_.empty()) {
        throw std::runtime_error("No asks in order book");
    }
    return asks_.front().price();
}

} // namespace mde::domain
