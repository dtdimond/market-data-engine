#pragma once

#include "domain/aggregates/OrderBook.hpp"
#include "repositories/IOrderBookRepository.hpp"
#include "services/IMarketDataFeed.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace mde::services {

class OrderBookService {
public:
    OrderBookService(mde::repositories::IOrderBookRepository& repo,
                     IMarketDataFeed& feed,
                     uint64_t snapshot_interval = 1000);

    // Lifecycle — delegates to feed
    void subscribe(const std::string& token_id);
    void start();
    void stop();

    // Event ingestion (also called by feed callback)
    void on_event(const mde::domain::OrderBookEventVariant& event);

    // Queries against current projection
    const mde::domain::OrderBook& get_current_book(const mde::domain::MarketAsset& asset) const;
    mde::domain::Spread get_current_spread(const mde::domain::MarketAsset& asset) const;
    mde::domain::Price get_midpoint(const mde::domain::MarketAsset& asset) const;

    // Asset resolution and event count
    std::optional<mde::domain::MarketAsset> resolve_asset(const std::string& token_id) const;
    uint64_t event_count() const;

private:
    void maybe_snapshot(const mde::domain::MarketAsset& asset, uint64_t sequence_number);

    mde::repositories::IOrderBookRepository& repository_;
    IMarketDataFeed& feed_;
    std::map<mde::domain::MarketAsset, mde::domain::OrderBook> current_books_;
    uint64_t snapshot_interval_;
    uint64_t next_sequence_number_{1};
};

} // namespace mde::services
