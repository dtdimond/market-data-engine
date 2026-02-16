#include "infrastructure/PolymarketMessageParser.hpp"

#include <gtest/gtest.h>

using namespace mde::domain;
using mde::infrastructure::PolymarketMessageParser;

class ParserTest : public ::testing::Test {
protected:
    PolymarketMessageParser parser;
};

// --- BookSnapshot ---

TEST_F(ParserTest, ParsesBookSnapshot) {
    auto events = parser.parse(R"([{
        "event_type": "book",
        "asset_id": "6581861",
        "market": "0xbd31dc",
        "bids": [
            {"price": "0.48", "size": "30"},
            {"price": "0.49", "size": "20"}
        ],
        "asks": [
            {"price": "0.52", "size": "25"},
            {"price": "0.53", "size": "60"}
        ],
        "timestamp": "1750428146322",
        "hash": "0xabc123"
    }])");

    ASSERT_EQ(events.size(), 1);
    auto& snap = std::get<BookSnapshot>(events[0]);

    EXPECT_EQ(snap.asset.condition_id(), "0xbd31dc");
    EXPECT_EQ(snap.asset.token_id(), "6581861");
    EXPECT_EQ(snap.timestamp.milliseconds(), 1750428146322);
    EXPECT_EQ(snap.hash, "0xabc123");
    EXPECT_EQ(snap.sequence_number, 0);

    ASSERT_EQ(snap.bids.size(), 2);
    EXPECT_DOUBLE_EQ(snap.bids[0].price().value(), 0.48);
    EXPECT_DOUBLE_EQ(snap.bids[0].size().size(), 30.0);
    EXPECT_DOUBLE_EQ(snap.bids[1].price().value(), 0.49);

    ASSERT_EQ(snap.asks.size(), 2);
    EXPECT_DOUBLE_EQ(snap.asks[0].price().value(), 0.52);
    EXPECT_DOUBLE_EQ(snap.asks[1].size().size(), 60.0);
}

TEST_F(ParserTest, ParsesBookSnapshotWithEmptyBook) {
    auto events = parser.parse(R"([{
        "event_type": "book",
        "asset_id": "6581861",
        "market": "0xbd31dc",
        "bids": [],
        "asks": [],
        "timestamp": "1000",
        "hash": ""
    }])");

    ASSERT_EQ(events.size(), 1);
    auto& snap = std::get<BookSnapshot>(events[0]);
    EXPECT_TRUE(snap.bids.empty());
    EXPECT_TRUE(snap.asks.empty());
}

// --- BookDelta (price_change) ---

TEST_F(ParserTest, ParsesPriceChange) {
    auto events = parser.parse(R"([{
        "event_type": "price_change",
        "market": "0xbd31dc",
        "timestamp": "1757908892351",
        "price_changes": [{
            "asset_id": "6581861",
            "price": "0.5",
            "size": "200",
            "side": "BUY",
            "hash": "56621a",
            "best_bid": "0.5",
            "best_ask": "0.52"
        }]
    }])");

    ASSERT_EQ(events.size(), 1);
    auto& delta = std::get<BookDelta>(events[0]);

    EXPECT_EQ(delta.asset.condition_id(), "0xbd31dc");
    EXPECT_EQ(delta.asset.token_id(), "6581861");
    EXPECT_EQ(delta.timestamp.milliseconds(), 1757908892351);

    ASSERT_EQ(delta.changes.size(), 1);
    EXPECT_DOUBLE_EQ(delta.changes[0].price.value(), 0.5);
    EXPECT_DOUBLE_EQ(delta.changes[0].new_size.size(), 200.0);
    EXPECT_EQ(delta.changes[0].side, Side::BUY);
    EXPECT_DOUBLE_EQ(delta.changes[0].best_bid.value(), 0.5);
    EXPECT_DOUBLE_EQ(delta.changes[0].best_ask.value(), 0.52);
}

TEST_F(ParserTest, ParsesPriceChangeWithMultipleAssets) {
    auto events = parser.parse(R"([{
        "event_type": "price_change",
        "market": "0xbd31dc",
        "timestamp": "1000",
        "price_changes": [
            {"asset_id": "111", "price": "0.5", "size": "100", "side": "BUY", "hash": "", "best_bid": "0.5", "best_ask": "0.6"},
            {"asset_id": "222", "price": "0.4", "size": "50", "side": "SELL", "hash": "", "best_bid": "0.3", "best_ask": "0.4"}
        ]
    }])");

    // Two different asset_ids → two BookDelta events
    ASSERT_EQ(events.size(), 2);
    EXPECT_TRUE(std::holds_alternative<BookDelta>(events[0]));
    EXPECT_TRUE(std::holds_alternative<BookDelta>(events[1]));
}

// --- TradeEvent (last_trade_price) ---

TEST_F(ParserTest, ParsesTradeEvent) {
    auto events = parser.parse(R"([{
        "event_type": "last_trade_price",
        "asset_id": "6581861",
        "market": "0xbd31dc",
        "price": "0.456",
        "side": "BUY",
        "size": "219.217767",
        "fee_rate_bps": "0",
        "timestamp": "1750428146322"
    }])");

    ASSERT_EQ(events.size(), 1);
    auto& trade = std::get<TradeEvent>(events[0]);

    EXPECT_EQ(trade.asset.condition_id(), "0xbd31dc");
    EXPECT_EQ(trade.asset.token_id(), "6581861");
    EXPECT_DOUBLE_EQ(trade.price.value(), 0.456);
    EXPECT_DOUBLE_EQ(trade.size.size(), 219.217767);
    EXPECT_EQ(trade.side, Side::BUY);
    EXPECT_EQ(trade.fee_rate_bps, "0");
}

TEST_F(ParserTest, ParsesSellTrade) {
    auto events = parser.parse(R"([{
        "event_type": "last_trade_price",
        "asset_id": "6581861",
        "market": "0xbd31dc",
        "price": "0.50",
        "side": "SELL",
        "size": "100",
        "fee_rate_bps": "200",
        "timestamp": "5000"
    }])");

    ASSERT_EQ(events.size(), 1);
    auto& trade = std::get<TradeEvent>(events[0]);
    EXPECT_EQ(trade.side, Side::SELL);
    EXPECT_EQ(trade.fee_rate_bps, "200");
}

// --- TickSizeChange ---

TEST_F(ParserTest, ParsesTickSizeChange) {
    auto events = parser.parse(R"([{
        "event_type": "tick_size_change",
        "asset_id": "6581861",
        "market": "0xbd31dc",
        "old_tick_size": "0.01",
        "new_tick_size": "0.001",
        "timestamp": "100000000"
    }])");

    ASSERT_EQ(events.size(), 1);
    auto& tick = std::get<TickSizeChange>(events[0]);

    EXPECT_EQ(tick.asset.condition_id(), "0xbd31dc");
    EXPECT_DOUBLE_EQ(tick.old_tick_size.value(), 0.01);
    EXPECT_DOUBLE_EQ(tick.new_tick_size.value(), 0.001);
}

// --- Edge cases ---

TEST_F(ParserTest, IgnoresUnknownEventType) {
    auto events = parser.parse(R"([{"event_type": "unknown", "foo": "bar"}])");
    EXPECT_TRUE(events.empty());
}

TEST_F(ParserTest, IgnoresObjectWithoutEventType) {
    auto events = parser.parse(R"([{"foo": "bar"}])");
    EXPECT_TRUE(events.empty());
}

TEST_F(ParserTest, HandlesNonArrayMessage) {
    auto events = parser.parse(R"({
        "event_type": "last_trade_price",
        "asset_id": "6581861",
        "market": "0xbd31dc",
        "price": "0.50",
        "side": "BUY",
        "size": "100",
        "fee_rate_bps": "0",
        "timestamp": "1000"
    })");

    ASSERT_EQ(events.size(), 1);
    EXPECT_TRUE(std::holds_alternative<TradeEvent>(events[0]));
}

TEST_F(ParserTest, ThrowsOnMalformedJson) {
    EXPECT_ANY_THROW(parser.parse("not json"));
}
