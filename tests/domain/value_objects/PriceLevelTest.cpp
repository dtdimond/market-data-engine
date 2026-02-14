#include "domain/value_objects/PriceLevel.hpp"

#include <gtest/gtest.h>

using mde::domain::Price;
using mde::domain::PriceLevel;
using mde::domain::Quantity;

TEST(PriceLevel, ConstructsWithPriceAndQuantity) {
    PriceLevel level(Price(0.48), Quantity(30.0));
    EXPECT_DOUBLE_EQ(level.price().value(), 0.48);
    EXPECT_DOUBLE_EQ(level.size().size(), 30.0);
}

TEST(PriceLevel, EqualLevelsAreEqual) {
    PriceLevel a(Price(0.48), Quantity(30.0));
    PriceLevel b(Price(0.48), Quantity(30.0));
    EXPECT_EQ(a, b);
}

TEST(PriceLevel, DifferentPricesAreNotEqual) {
    PriceLevel a(Price(0.48), Quantity(30.0));
    PriceLevel b(Price(0.52), Quantity(30.0));
    EXPECT_NE(a, b);
}

TEST(PriceLevel, DifferentSizesAreNotEqual) {
    PriceLevel a(Price(0.48), Quantity(30.0));
    PriceLevel b(Price(0.48), Quantity(50.0));
    EXPECT_NE(a, b);
}

TEST(PriceLevel, OrdersByPriceFirst) {
    PriceLevel low(Price(0.30), Quantity(100.0));
    PriceLevel high(Price(0.70), Quantity(10.0));
    EXPECT_LT(low, high);
    EXPECT_GT(high, low);
}

TEST(PriceLevel, OrdersBySizeWhenPriceEqual) {
    PriceLevel small(Price(0.50), Quantity(10.0));
    PriceLevel large(Price(0.50), Quantity(100.0));
    EXPECT_LT(small, large);
}

TEST(PriceLevel, FromStringsParsesCorrectly) {
    PriceLevel level = PriceLevel::from_strings("0.48", "30");
    EXPECT_DOUBLE_EQ(level.price().value(), 0.48);
    EXPECT_DOUBLE_EQ(level.size().size(), 30.0);
}

TEST(PriceLevel, FromStringsThrowsOnInvalidPrice) {
    EXPECT_THROW(PriceLevel::from_strings("1.5", "30"), std::out_of_range);
}

TEST(PriceLevel, FromStringsThrowsOnInvalidSize) {
    EXPECT_THROW(PriceLevel::from_strings("0.5", "-10"), std::out_of_range);
}

TEST(PriceLevel, CopySemantics) {
    PriceLevel original(Price(0.52), Quantity(25.0));
    PriceLevel copy = original;
    EXPECT_EQ(original, copy);
    EXPECT_DOUBLE_EQ(copy.price().value(), 0.52);
    EXPECT_DOUBLE_EQ(copy.size().size(), 25.0);
}
