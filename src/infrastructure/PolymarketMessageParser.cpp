#include "infrastructure/PolymarketMessageParser.hpp"

#include <nlohmann/json.hpp>

#include <map>

using json = nlohmann::json;
using namespace mde::domain;

namespace mde::infrastructure {

namespace {

BookSnapshot parse_book_snapshot(const json& obj) {
    auto market = obj["market"].get<std::string>();
    auto asset_id = obj["asset_id"].get<std::string>();
    auto timestamp = Timestamp::from_string(obj["timestamp"].get<std::string>());
    auto hash = obj.value("hash", "");

    std::vector<PriceLevel> bids;
    for (const auto& level : obj["bids"]) {
        bids.push_back(PriceLevel::from_strings(
            level["price"].get<std::string>(),
            level["size"].get<std::string>()));
    }

    std::vector<PriceLevel> asks;
    for (const auto& level : obj["asks"]) {
        asks.push_back(PriceLevel::from_strings(
            level["price"].get<std::string>(),
            level["size"].get<std::string>()));
    }

    return BookSnapshot{
        {MarketAsset(market, asset_id), timestamp, 0},
        std::move(bids), std::move(asks), hash
    };
}

// price_change can contain changes for multiple assets,
// so we group by asset_id and return one BookDelta per asset.
std::vector<OrderBookEventVariant> parse_price_change(const json& obj) {
    auto market = obj["market"].get<std::string>();
    auto timestamp = Timestamp::from_string(obj["timestamp"].get<std::string>());

    // Group changes by asset_id
    std::map<std::string, std::vector<PriceLevelDelta>> by_asset;
    for (const auto& change : obj["price_changes"]) {
        auto asset_id = change["asset_id"].get<std::string>();
        by_asset[asset_id].push_back(PriceLevelDelta{
            asset_id,
            Price::from_string(change["price"].get<std::string>()),
            Quantity::from_string(change["size"].get<std::string>()),
            side_from_string(change["side"].get<std::string>()),
            Price::from_string(change["best_bid"].get<std::string>()),
            Price::from_string(change["best_ask"].get<std::string>()),
        });
    }

    std::vector<OrderBookEventVariant> events;
    for (auto& [asset_id, changes] : by_asset) {
        events.push_back(BookDelta{
            {MarketAsset(market, asset_id), timestamp, 0},
            std::move(changes)
        });
    }
    return events;
}

TradeEvent parse_trade_event(const json& obj) {
    auto market = obj["market"].get<std::string>();
    auto asset_id = obj["asset_id"].get<std::string>();
    auto timestamp = Timestamp::from_string(obj["timestamp"].get<std::string>());

    return TradeEvent{
        {MarketAsset(market, asset_id), timestamp, 0},
        Price::from_string(obj["price"].get<std::string>()),
        Quantity::from_string(obj["size"].get<std::string>()),
        side_from_string(obj["side"].get<std::string>()),
        obj.value("fee_rate_bps", "0")
    };
}

TickSizeChange parse_tick_size_change(const json& obj) {
    auto market = obj["market"].get<std::string>();
    auto asset_id = obj["asset_id"].get<std::string>();
    auto timestamp = Timestamp::from_string(obj["timestamp"].get<std::string>());

    return TickSizeChange{
        {MarketAsset(market, asset_id), timestamp, 0},
        Price::from_string(obj["old_tick_size"].get<std::string>()),
        Price::from_string(obj["new_tick_size"].get<std::string>())
    };
}

} // anonymous namespace

std::vector<OrderBookEventVariant> PolymarketMessageParser::parse(
    const std::string& json_str) const {

    auto json_msg = json::parse(json_str);
    std::vector<OrderBookEventVariant> events;

    // Polymarket wraps messages in a JSON array
    auto& items = json_msg.is_array() ? json_msg : (json_msg = json::array({json_msg}));

    for (const auto& obj : items) {
        if (!obj.is_object() || !obj.contains("event_type")) continue;

        auto event_type = obj["event_type"].get<std::string>();

        if (event_type == "book") {
            events.push_back(parse_book_snapshot(obj));
        } else if (event_type == "price_change") {
            auto deltas = parse_price_change(obj);
            events.insert(events.end(),
                          std::make_move_iterator(deltas.begin()),
                          std::make_move_iterator(deltas.end()));
        } else if (event_type == "last_trade_price") {
            events.push_back(parse_trade_event(obj));
        } else if (event_type == "tick_size_change") {
            events.push_back(parse_tick_size_change(obj));
        }
    }

    return events;
}

} // namespace mde::infrastructure
