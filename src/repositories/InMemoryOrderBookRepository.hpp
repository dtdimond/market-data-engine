#pragma once

#include "repositories/IOrderBookRepository.hpp"

#include <map>
#include <variant>
#include <vector>

namespace mde::repositories {

class InMemoryOrderBookRepository : public mde::repositories::IOrderBookRepository {
public:
    void append_event(const mde::domain::OrderBookEventVariant& event) override {
        events_.push_back(event);
    }

    std::vector<mde::domain::OrderBookEventVariant> get_events_since(
        const mde::domain::MarketAsset& asset, uint64_t sequence_number) const override {
        std::vector<mde::domain::OrderBookEventVariant> result;
        for (const auto& event : events_) {
            const auto& e_asset = std::visit(
                [](const auto& e) -> const mde::domain::MarketAsset& { return e.asset; }, event);
            auto e_seq = std::visit(
                [](const auto& e) { return e.sequence_number; }, event);
            if (e_asset == asset && e_seq > sequence_number) {
                result.push_back(event);
            }
        }
        return result;
    }

    void store_snapshot(const mde::domain::OrderBook& book) override {
        snapshots_.insert_or_assign(book.get_asset(), book);
    }

    std::optional<mde::domain::OrderBook> get_latest_snapshot(
        const mde::domain::MarketAsset& asset) const override {
        auto it = snapshots_.find(asset);
        if (it != snapshots_.end()) return it->second;
        return std::nullopt;
    }

    // Test helpers
    size_t event_count() const { return events_.size(); }
    const std::vector<mde::domain::OrderBookEventVariant>& events() const { return events_; }
    bool has_snapshot(const mde::domain::MarketAsset& asset) const {
        return snapshots_.count(asset) > 0;
    }

private:
    std::vector<mde::domain::OrderBookEventVariant> events_;
    std::map<mde::domain::MarketAsset, mde::domain::OrderBook> snapshots_;
};

} // namespace mde::repositories
