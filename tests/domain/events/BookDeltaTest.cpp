#include "domain/events/BookDelta.hpp"

#include <gtest/gtest.h>

using namespace mde::domain;

TEST(BookDelta, StoresBaseEventFields) {
    BookDelta delta{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(1757908892351), 5},
        {}
    };

    EXPECT_EQ(delta.asset.condition_id(), "0xbd31dc");
    EXPECT_EQ(delta.timestamp.milliseconds(), 1757908892351);
    EXPECT_EQ(delta.sequence_number, 5);
}

TEST(BookDelta, StoresPriceLevelDeltas) {
    BookDelta delta{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {
            PriceLevelDelta{
                "6581861",
                Price(0.50), Quantity(200.0), Side::BUY,
                Price(0.50), Price(0.52),
            },
        }
    };

    EXPECT_EQ(delta.changes.size(), 1);
    EXPECT_EQ(delta.changes[0].asset_id, "6581861");
    EXPECT_DOUBLE_EQ(delta.changes[0].price.value(), 0.50);
    EXPECT_DOUBLE_EQ(delta.changes[0].new_size.size(), 200.0);
    EXPECT_EQ(delta.changes[0].side, Side::BUY);
    EXPECT_DOUBLE_EQ(delta.changes[0].best_bid.value(), 0.50);
    EXPECT_DOUBLE_EQ(delta.changes[0].best_ask.value(), 0.52);
}

TEST(BookDelta, SupportsMultipleChanges) {
    BookDelta delta{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        {
            PriceLevelDelta{
                "6581861",
                Price(0.50), Quantity(200.0), Side::BUY,
                Price(0.50), Price(0.52),
            },
            PriceLevelDelta{
                "6581861",
                Price(0.52), Quantity(0.0), Side::SELL,
                Price(0.50), Price(0.53),
            },
        }
    };

    EXPECT_EQ(delta.changes.size(), 2);
    EXPECT_EQ(delta.changes[1].side, Side::SELL);
    EXPECT_DOUBLE_EQ(delta.changes[1].new_size.size(), 0.0);
}
