#include "domain/events/BookSnapshot.hpp"

#include <gtest/gtest.h>

using namespace mde::domain;

TEST(BookSnapshot, StoresBaseEventFields) {
    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(1750428146322), 1},
        {}, {}, ""
    };

    EXPECT_EQ(snap.asset.condition_id(), "0xbd31dc");
    EXPECT_EQ(snap.asset.token_id(), "6581861");
    EXPECT_EQ(snap.timestamp.milliseconds(), 1750428146322);
    EXPECT_EQ(snap.sequence_number, 1);
}

TEST(BookSnapshot, StoresBidsAndAsks) {
    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {
            PriceLevel(Price(0.48), Quantity(30.0)),
            PriceLevel(Price(0.49), Quantity(20.0)),
        },
        {
            PriceLevel(Price(0.52), Quantity(25.0)),
            PriceLevel(Price(0.53), Quantity(60.0)),
        },
        "0xabc123"
    };

    EXPECT_EQ(snap.bids.size(), 2);
    EXPECT_EQ(snap.asks.size(), 2);
    EXPECT_DOUBLE_EQ(snap.bids[0].price().value(), 0.48);
    EXPECT_DOUBLE_EQ(snap.asks[1].size().size(), 60.0);
    EXPECT_EQ(snap.hash, "0xabc123");
}

TEST(BookSnapshot, EmptyBookHasNoLevels) {
    BookSnapshot snap{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {}, {}, ""
    };

    EXPECT_TRUE(snap.bids.empty());
    EXPECT_TRUE(snap.asks.empty());
}
