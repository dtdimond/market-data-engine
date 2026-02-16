#include "domain/aggregates/OrderBook.hpp"

#include <gtest/gtest.h>

using namespace mde::domain;

// --- Factory ---

TEST(OrderBook, EmptyBookHasNoLevels) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    EXPECT_EQ(book.get_depth(), 0);
    EXPECT_TRUE(book.get_bids().empty());
    EXPECT_TRUE(book.get_asks().empty());
    EXPECT_EQ(book.get_last_sequence_number(), 0);
}

TEST(OrderBook, EmptyBookThrowsOnBestBid) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));
    EXPECT_THROW(book.get_best_bid(), std::runtime_error);
}

TEST(OrderBook, EmptyBookThrowsOnBestAsk) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));
    EXPECT_THROW(book.get_best_ask(), std::runtime_error);
}

TEST(OrderBook, EmptyBookDefaultTickSize) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));
    EXPECT_DOUBLE_EQ(book.get_tick_size().value(), 0.01);
}

// --- Apply BookSnapshot ---

TEST(OrderBook, ApplyBookSnapshotReplacesEntireBook) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(1000), 1},
        {PriceLevel(Price(0.48), Quantity(30.0)), PriceLevel(Price(0.49), Quantity(20.0))},
        {PriceLevel(Price(0.52), Quantity(25.0)), PriceLevel(Price(0.53), Quantity(60.0))},
        "0xabc"
    };

    auto updated = book.apply(snap);

    EXPECT_EQ(updated.get_depth(), 2);
    EXPECT_DOUBLE_EQ(updated.get_best_bid().value(), 0.49);
    EXPECT_DOUBLE_EQ(updated.get_best_ask().value(), 0.52);
    EXPECT_EQ(updated.get_book_hash(), "0xabc");
    EXPECT_EQ(updated.get_last_sequence_number(), 1);
    EXPECT_EQ(updated.get_timestamp().milliseconds(), 1000);
}

TEST(OrderBook, ApplyBookSnapshotSortsBids) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.30), Quantity(10.0)), PriceLevel(Price(0.49), Quantity(20.0)),
         PriceLevel(Price(0.40), Quantity(15.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };

    auto updated = book.apply(snap);

    // Bids should be sorted descending
    EXPECT_DOUBLE_EQ(updated.get_bids()[0].price().value(), 0.49);
    EXPECT_DOUBLE_EQ(updated.get_bids()[1].price().value(), 0.40);
    EXPECT_DOUBLE_EQ(updated.get_bids()[2].price().value(), 0.30);
}

TEST(OrderBook, ApplyBookSnapshotSortsAsks) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.60), Quantity(10.0)), PriceLevel(Price(0.52), Quantity(25.0)),
         PriceLevel(Price(0.55), Quantity(5.0))},
        ""
    };

    auto updated = book.apply(snap);

    // Asks should be sorted ascending
    EXPECT_DOUBLE_EQ(updated.get_asks()[0].price().value(), 0.52);
    EXPECT_DOUBLE_EQ(updated.get_asks()[1].price().value(), 0.55);
    EXPECT_DOUBLE_EQ(updated.get_asks()[2].price().value(), 0.60);
}

TEST(OrderBook, ApplySnapshotIsImmutable) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };

    auto updated = book.apply(snap);

    // Original book is unchanged
    EXPECT_EQ(book.get_depth(), 0);
    EXPECT_EQ(updated.get_depth(), 1);
}

// --- Apply BookDelta ---

TEST(OrderBook, ApplyBookDeltaAddsNewBidLevel) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };
    book = book.apply(snap);

    BookDelta delta{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(100), 2},
        {PriceLevelDelta{"6581861", Price(0.50), Quantity(100.0), Side::BUY,
                         Price(0.50), Price(0.52)}}
    };

    auto updated = book.apply(delta);

    EXPECT_EQ(updated.get_depth(), 2);
    EXPECT_DOUBLE_EQ(updated.get_best_bid().value(), 0.50);
    EXPECT_EQ(updated.get_last_sequence_number(), 2);
}

TEST(OrderBook, ApplyBookDeltaUpdatesExistingLevel) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };
    book = book.apply(snap);

    BookDelta delta{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(100), 2},
        {PriceLevelDelta{"6581861", Price(0.48), Quantity(50.0), Side::BUY,
                         Price(0.48), Price(0.52)}}
    };

    auto updated = book.apply(delta);

    EXPECT_EQ(updated.get_depth(), 1);
    EXPECT_DOUBLE_EQ(updated.get_bids()[0].size().size(), 50.0);
}

TEST(OrderBook, ApplyBookDeltaRemovesLevelWhenSizeZero) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0)), PriceLevel(Price(0.47), Quantity(20.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };
    book = book.apply(snap);

    BookDelta delta{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(100), 2},
        {PriceLevelDelta{"6581861", Price(0.48), Quantity(0.0), Side::BUY,
                         Price(0.47), Price(0.52)}}
    };

    auto updated = book.apply(delta);

    EXPECT_EQ(updated.get_bids().size(), 1);
    EXPECT_DOUBLE_EQ(updated.get_best_bid().value(), 0.47);
}

TEST(OrderBook, ApplyBookDeltaAddsAskLevel) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };
    book = book.apply(snap);

    BookDelta delta{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(100), 2},
        {PriceLevelDelta{"6581861", Price(0.51), Quantity(10.0), Side::SELL,
                         Price(0.48), Price(0.51)}}
    };

    auto updated = book.apply(delta);

    EXPECT_DOUBLE_EQ(updated.get_best_ask().value(), 0.51);
    EXPECT_EQ(updated.get_asks().size(), 2);
}

// --- Apply TradeEvent ---

TEST(OrderBook, ApplyTradeRecordsLatestTrade) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    TradeEvent trade{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(5000), 1},
        Price(0.456), Quantity(219.22), Side::BUY, "0"
    };

    auto updated = book.apply(trade);

    ASSERT_TRUE(updated.get_latest_trade().has_value());
    EXPECT_DOUBLE_EQ(updated.get_latest_trade()->price.value(), 0.456);
    EXPECT_DOUBLE_EQ(updated.get_latest_trade()->size.size(), 219.22);
    EXPECT_EQ(updated.get_latest_trade()->side, Side::BUY);
}

TEST(OrderBook, ApplyTradePreservesBookLevels) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };
    book = book.apply(snap);

    TradeEvent trade{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(5000), 2},
        Price(0.50), Quantity(10.0), Side::BUY, "0"
    };

    auto updated = book.apply(trade);

    // Book levels unchanged — snapshot follows separately
    EXPECT_EQ(updated.get_depth(), 1);
    EXPECT_DOUBLE_EQ(updated.get_best_bid().value(), 0.48);
    ASSERT_TRUE(updated.get_latest_trade().has_value());
}

// --- Apply TickSizeChange ---

TEST(OrderBook, ApplyTickSizeChangeUpdatesTickSize) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    TickSizeChange event{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(1000), 1},
        Price(0.01), Price(0.001)
    };

    auto updated = book.apply(event);

    EXPECT_DOUBLE_EQ(updated.get_tick_size().value(), 0.001);
    EXPECT_EQ(updated.get_last_sequence_number(), 1);
}

// --- Queries ---

TEST(OrderBook, GetSpread) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };

    auto updated = book.apply(snap);
    auto spread = updated.get_spread();

    EXPECT_DOUBLE_EQ(spread.best_bid.value(), 0.48);
    EXPECT_DOUBLE_EQ(spread.best_ask.value(), 0.52);
    EXPECT_NEAR(spread.value(), 0.04, 1e-10);
}

TEST(OrderBook, GetMidpoint) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };

    auto updated = book.apply(snap);

    EXPECT_DOUBLE_EQ(updated.get_midpoint().value(), 0.50);
}

// --- Variant dispatch ---

TEST(OrderBook, ApplyVariantDispatchesCorrectly) {
    auto book = OrderBook::empty(MarketAsset("0xbd31dc", "6581861"));

    OrderBookEventVariant event = BookSnapshot{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {PriceLevel(Price(0.48), Quantity(30.0))},
        {PriceLevel(Price(0.52), Quantity(25.0))},
        ""
    };

    auto updated = book.apply(event);

    EXPECT_EQ(updated.get_depth(), 1);
    EXPECT_DOUBLE_EQ(updated.get_best_bid().value(), 0.48);
}
