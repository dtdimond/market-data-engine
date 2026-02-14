#include "domain/events/TickSizeChange.hpp"

#include <gtest/gtest.h>

using namespace mde::domain;

TEST(TickSizeChange, StoresTickSizeFields) {
    TickSizeChange event{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(100000000), 42},
        Price(0.01), Price(0.001)
    };

    EXPECT_EQ(event.asset.condition_id(), "0xbd31dc");
    EXPECT_EQ(event.sequence_number, 42);
    EXPECT_DOUBLE_EQ(event.old_tick_size.value(), 0.01);
    EXPECT_DOUBLE_EQ(event.new_tick_size.value(), 0.001);
}

TEST(TickSizeChange, TickSizeIncrease) {
    TickSizeChange event{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        Price(0.001), Price(0.01)
    };

    EXPECT_GT(event.new_tick_size, event.old_tick_size);
}
