#include "services/OrderBookService.hpp"

#include <stdexcept>
#include <variant>

using namespace mde::domain;

namespace mde::services {

OrderBookService::OrderBookService(mde::repositories::IOrderBookRepository& repo,
                                   IMarketDataFeed& feed,
                                   uint64_t snapshot_interval)
    : repository_(repo)
    , feed_(feed)
    , snapshot_interval_(snapshot_interval) {
    feed_.set_on_event([this](const OrderBookEventVariant& event) {
        on_event(event);
    });
}

void OrderBookService::subscribe(const std::string& token_id) {
    feed_.subscribe(token_id);
}

void OrderBookService::start() {
    feed_.start();
}

void OrderBookService::stop() {
    feed_.stop();
}

void OrderBookService::on_event(const OrderBookEventVariant& event) {
    // Assign sequence number
    auto numbered = event;
    std::visit([this](auto& e) { e.sequence_number = next_sequence_number_++; }, numbered);

    // Extract asset from event
    const auto& asset = std::visit(
        [](const auto& e) -> const MarketAsset& { return e.asset; }, numbered);

    // Persist event
    repository_.append_event(numbered);

    // Find or create the book for this asset
    auto it = current_books_.find(asset);
    if (it == current_books_.end()) {
        it = current_books_.emplace(asset, OrderBook::empty(asset)).first;
    }

    // Apply event to projection
    it->second = it->second.apply(numbered);

    // Maybe persist a snapshot
    maybe_snapshot(asset, it->second.get_last_sequence_number());
}

const OrderBook& OrderBookService::get_current_book(const MarketAsset& asset) const {
    auto it = current_books_.find(asset);
    if (it == current_books_.end()) {
        throw std::runtime_error("No book for asset");
    }
    return it->second;
}

Spread OrderBookService::get_current_spread(const MarketAsset& asset) const {
    return get_current_book(asset).get_spread();
}

Price OrderBookService::get_midpoint(const MarketAsset& asset) const {
    return get_current_book(asset).get_midpoint();
}

std::optional<MarketAsset> OrderBookService::resolve_asset(const std::string& token_id) const {
    for (const auto& [asset, book] : current_books_) {
        if (asset.token_id() == token_id) {
            return asset;
        }
    }
    return std::nullopt;
}

uint64_t OrderBookService::event_count() const {
    return next_sequence_number_ - 1;
}

size_t OrderBookService::book_count() const {
    return current_books_.size();
}

void OrderBookService::maybe_snapshot(const MarketAsset& asset, uint64_t sequence_number) {
    if (snapshot_interval_ > 0 && sequence_number % snapshot_interval_ == 0) {
        auto it = current_books_.find(asset);
        if (it != current_books_.end()) {
            repository_.store_snapshot(it->second);
        }
    }
}

} // namespace mde::services
