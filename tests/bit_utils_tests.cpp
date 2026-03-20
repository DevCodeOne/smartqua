#include <gtest/gtest.h>

#include "utils/bit_utils.h"

TEST(combineFlags, AllValuesInRangeReturnsCorrectValue) {
    EXPECT_EQ(setFlagsAtPos<uint8_t>(0), 1);
    EXPECT_EQ(setFlagsAtPos<uint8_t>(1), 2);
    EXPECT_EQ(setFlagsAtPos<uint8_t>(0, 1), 3);
}

TEST(combineFlags, NegativeValueReturnsEmptyOptional) {
    EXPECT_FALSE(setFlagsAtPos<uint8_t>(-1).has_value());
    EXPECT_FALSE(setFlagsAtPos<uint8_t>(0, 7, -1).has_value());
}

TEST(combineFlags, OverflowWithSomeValuesReturnsEmptyOptional) {
    EXPECT_FALSE(setFlagsAtPos<uint8_t>(8).has_value());
    EXPECT_FALSE(setFlagsAtPos<uint8_t>(0, 8).has_value());
}