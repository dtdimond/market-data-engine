#include "domain/value_objects/Price.hpp"

#include <gtest/gtest.h>

using mde::domain::Price;

TEST(Price, ConstructsWithValidProbability) {
    Price p(0.5);
    EXPECT_DOUBLE_EQ(p.value(), 0.5);
}

TEST(Price, ConstructsAtZero) {
    Price p(0.0);
    EXPECT_DOUBLE_EQ(p.value(), 0.0);
}

TEST(Price, ConstructsAtOne) {
    Price p(1.0);
    EXPECT_DOUBLE_EQ(p.value(), 1.0);
}

TEST(Price, ThrowsOnNegativeValue) {
    EXPECT_THROW(Price(-0.01), std::out_of_range);
}

TEST(Price, ThrowsOnValueAboveOne) {
    EXPECT_THROW(Price(1.01), std::out_of_range);
}

TEST(Price, EqualPricesAreEqual) {
    Price a(0.48);
    Price b(0.48);
    EXPECT_EQ(a, b);
}

TEST(Price, DifferentPricesAreNotEqual) {
    Price a(0.48);
    Price b(0.52);
    EXPECT_NE(a, b);
}

TEST(Price, OrdersByValue) {
    Price low(0.30);
    Price high(0.70);
    EXPECT_LT(low, high);
    EXPECT_GT(high, low);
    EXPECT_LE(low, high);
    EXPECT_GE(high, low);
}

TEST(Price, EqualPricesCompareAsEqual) {
    Price a(0.50);
    Price b(0.50);
    EXPECT_LE(a, b);
    EXPECT_GE(a, b);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
}

TEST(Price, FromStringParsesDecimal) {
    Price p = Price::from_string("0.456");
    EXPECT_DOUBLE_EQ(p.value(), 0.456);
}

TEST(Price, FromStringThrowsOnInvalidRange) {
    EXPECT_THROW(Price::from_string("1.5"), std::out_of_range);
}

TEST(Price, FromStringThrowsOnNonNumeric) {
    EXPECT_THROW(Price::from_string("abc"), std::invalid_argument);
}

TEST(Price, ZeroReturnsZeroPrice) {
    Price p = Price::zero();
    EXPECT_DOUBLE_EQ(p.value(), 0.0);
}

TEST(Price, CopySemantics) {
    Price original(0.75);
    Price copy = original;
    EXPECT_EQ(original, copy);
    EXPECT_DOUBLE_EQ(copy.value(), 0.75);
}
