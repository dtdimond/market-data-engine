#include "domain/value_objects/Quantity.hpp"

#include <gtest/gtest.h>

using mde::domain::Quantity;

TEST(Quantity, ConstructsWithValidSize) {
    Quantity q(100.5);
    EXPECT_DOUBLE_EQ(q.size(), 100.5);
}

TEST(Quantity, ConstructsAtZero) {
    Quantity q(0.0);
    EXPECT_DOUBLE_EQ(q.size(), 0.0);
}

TEST(Quantity, ConstructsWithLargeValue) {
    Quantity q(1000000.0);
    EXPECT_DOUBLE_EQ(q.size(), 1000000.0);
}

TEST(Quantity, ThrowsOnNegativeValue) {
    EXPECT_THROW(Quantity(-0.01), std::out_of_range);
}

TEST(Quantity, EqualQuantitiesAreEqual) {
    Quantity a(219.22);
    Quantity b(219.22);
    EXPECT_EQ(a, b);
}

TEST(Quantity, DifferentQuantitiesAreNotEqual) {
    Quantity a(100.0);
    Quantity b(200.0);
    EXPECT_NE(a, b);
}

TEST(Quantity, OrdersBySize) {
    Quantity small(30.0);
    Quantity large(200.0);
    EXPECT_LT(small, large);
    EXPECT_GT(large, small);
    EXPECT_LE(small, large);
    EXPECT_GE(large, small);
}

TEST(Quantity, EqualQuantitiesCompareAsEqual) {
    Quantity a(50.0);
    Quantity b(50.0);
    EXPECT_LE(a, b);
    EXPECT_GE(a, b);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
}

TEST(Quantity, FromStringParsesDecimal) {
    Quantity q = Quantity::from_string("219.217767");
    EXPECT_DOUBLE_EQ(q.size(), 219.217767);
}

TEST(Quantity, FromStringParsesInteger) {
    Quantity q = Quantity::from_string("30");
    EXPECT_DOUBLE_EQ(q.size(), 30.0);
}

TEST(Quantity, FromStringThrowsOnNegative) {
    EXPECT_THROW(Quantity::from_string("-10"), std::out_of_range);
}

TEST(Quantity, FromStringThrowsOnNonNumeric) {
    EXPECT_THROW(Quantity::from_string("abc"), std::invalid_argument);
}

TEST(Quantity, ZeroReturnsZeroQuantity) {
    Quantity q = Quantity::zero();
    EXPECT_DOUBLE_EQ(q.size(), 0.0);
}

TEST(Quantity, CopySemantics) {
    Quantity original(75.5);
    Quantity copy = original;
    EXPECT_EQ(original, copy);
    EXPECT_DOUBLE_EQ(copy.size(), 75.5);
}
