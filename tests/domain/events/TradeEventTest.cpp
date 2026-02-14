#include "domain/events/TradeEvent.hpp"

#include <gtest/gtest.h>

using namespace mde::domain;

TEST(TradeEvent, StoresTradeFields) {
    TradeEvent trade{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(1750428146322), 10},
        Price(0.456), Quantity(219.217767), Side::BUY, "0"
    };

    EXPECT_EQ(trade.asset.token_id(), "6581861");
    EXPECT_EQ(trade.timestamp.milliseconds(), 1750428146322);
    EXPECT_EQ(trade.sequence_number, 10);
    EXPECT_DOUBLE_EQ(trade.price.value(), 0.456);
    EXPECT_DOUBLE_EQ(trade.size.size(), 219.217767);
    EXPECT_EQ(trade.side, Side::BUY);
    EXPECT_EQ(trade.fee_rate_bps, "0");
}

TEST(TradeEvent, SellSide) {
    TradeEvent trade{
        {MarketAsset("0xbd31dc", "6581861"), Timestamp(0), 1},
        Price(0.50), Quantity(100.0), Side::SELL, "200"
    };

    EXPECT_EQ(trade.side, Side::SELL);
    EXPECT_EQ(trade.fee_rate_bps, "200");
}
