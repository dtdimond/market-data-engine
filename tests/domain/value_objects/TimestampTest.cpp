#include "domain/value_objects/Timestamp.hpp"

#include <gtest/gtest.h>

using mde::domain::Timestamp;

TEST(Timestamp, ConstructsWithValidMilliseconds) {
    Timestamp ts(1750428146322);
    EXPECT_EQ(ts.milliseconds(), 1750428146322);
}

TEST(Timestamp, ConstructsAtZero) {
    Timestamp ts(0);
    EXPECT_EQ(ts.milliseconds(), 0);
}

TEST(Timestamp, ThrowsOnNegativeValue) {
    EXPECT_THROW(Timestamp(-1), std::out_of_range);
}

TEST(Timestamp, EqualTimestampsAreEqual) {
    Timestamp a(1000);
    Timestamp b(1000);
    EXPECT_EQ(a, b);
}

TEST(Timestamp, DifferentTimestampsAreNotEqual) {
    Timestamp a(1000);
    Timestamp b(2000);
    EXPECT_NE(a, b);
}

TEST(Timestamp, OrdersByMilliseconds) {
    Timestamp earlier(1000);
    Timestamp later(2000);
    EXPECT_LT(earlier, later);
    EXPECT_GT(later, earlier);
    EXPECT_LE(earlier, later);
    EXPECT_GE(later, earlier);
}

TEST(Timestamp, EqualTimestampsCompareAsEqual) {
    Timestamp a(5000);
    Timestamp b(5000);
    EXPECT_LE(a, b);
    EXPECT_GE(a, b);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
}

TEST(Timestamp, FromStringParsesMilliseconds) {
    Timestamp ts = Timestamp::from_string("1750428146322");
    EXPECT_EQ(ts.milliseconds(), 1750428146322);
}

TEST(Timestamp, FromStringThrowsOnNegative) {
    EXPECT_THROW(Timestamp::from_string("-100"), std::out_of_range);
}

TEST(Timestamp, FromStringThrowsOnNonNumeric) {
    EXPECT_THROW(Timestamp::from_string("abc"), std::invalid_argument);
}

TEST(Timestamp, CopySemantics) {
    Timestamp original(123456789000);
    Timestamp copy = original;
    EXPECT_EQ(original, copy);
    EXPECT_EQ(copy.milliseconds(), 123456789000);
}
