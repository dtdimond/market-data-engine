#include "domain/value_objects/Side.hpp"

#include <gtest/gtest.h>

using mde::domain::Side;
using mde::domain::side_from_string;

TEST(Side, BuyAndSellAreDifferent) {
    EXPECT_NE(Side::BUY, Side::SELL);
}

TEST(Side, FromStringParsesBuy) {
    EXPECT_EQ(side_from_string("BUY"), Side::BUY);
}

TEST(Side, FromStringParsesSell) {
    EXPECT_EQ(side_from_string("SELL"), Side::SELL);
}

TEST(Side, FromStringThrowsOnInvalid) {
    EXPECT_THROW(side_from_string("buy"), std::invalid_argument);
    EXPECT_THROW(side_from_string(""), std::invalid_argument);
    EXPECT_THROW(side_from_string("HOLD"), std::invalid_argument);
}
