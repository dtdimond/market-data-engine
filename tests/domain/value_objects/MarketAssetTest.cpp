#include "domain/value_objects/MarketAsset.hpp"

#include <gtest/gtest.h>

using mde::domain::MarketAsset;

TEST(MarketAsset, ConstructsWithValidIds) {
    MarketAsset asset("0xbd31dc", "6581861");
    EXPECT_EQ(asset.condition_id(), "0xbd31dc");
    EXPECT_EQ(asset.token_id(), "6581861");
}

TEST(MarketAsset, ThrowsOnEmptyConditionId) {
    EXPECT_THROW(MarketAsset("", "6581861"), std::invalid_argument);
}

TEST(MarketAsset, ThrowsOnEmptyTokenId) {
    EXPECT_THROW(MarketAsset("0xbd31dc", ""), std::invalid_argument);
}

TEST(MarketAsset, EqualAssetsAreEqual) {
    MarketAsset a("0xbd31dc", "6581861");
    MarketAsset b("0xbd31dc", "6581861");
    EXPECT_EQ(a, b);
}

TEST(MarketAsset, DifferentConditionIdsAreNotEqual) {
    MarketAsset a("0xbd31dc", "6581861");
    MarketAsset b("0xaaaaaa", "6581861");
    EXPECT_NE(a, b);
}

TEST(MarketAsset, DifferentTokenIdsAreNotEqual) {
    MarketAsset a("0xbd31dc", "6581861");
    MarketAsset b("0xbd31dc", "9999999");
    EXPECT_NE(a, b);
}

TEST(MarketAsset, OrdersByConditionIdFirst) {
    MarketAsset a("0xaaa", "999");
    MarketAsset b("0xbbb", "111");
    EXPECT_LT(a, b);
}

TEST(MarketAsset, OrdersByTokenIdWhenConditionIdEqual) {
    MarketAsset a("0xaaa", "111");
    MarketAsset b("0xaaa", "222");
    EXPECT_LT(a, b);
}

TEST(MarketAsset, CopySemantics) {
    MarketAsset original("0xbd31dc", "6581861");
    MarketAsset copy = original;
    EXPECT_EQ(original, copy);
    EXPECT_EQ(copy.condition_id(), "0xbd31dc");
    EXPECT_EQ(copy.token_id(), "6581861");
}
