#include "utils/stack_string.h"

#include <gtest/gtest.h>

TEST(BasicStackStringTests, canHold_validSize) {
    BasicStackString<10> string;
    EXPECT_TRUE(string.canHold("12345"));
}

TEST(BasicStackStringTests, canHold_JustSize) {
    BasicStackString<5> string;
    EXPECT_TRUE(string.canHold("1234"));
}

TEST(BasicStackStringTests, canHold_exceedsSize) {
    BasicStackString<5> string;
    EXPECT_FALSE(string.canHold("123456"));
}

TEST(BasicStackStringTests, clear) {
    BasicStackString<10> string("12345");
    string.clear();
    EXPECT_EQ(string.len(), 0);
    EXPECT_EQ(string.capacity(), 10);
}

TEST(BasicStackStringTests, appendWithinCapacity) {
    BasicStackString<10> string("1234");
    EXPECT_TRUE(string.append("56"));
    EXPECT_EQ(string.len(), 6);
    EXPECT_EQ(string.data()[string.len()], '\0');
    EXPECT_EQ(string.getStringView(), "123456");
}

TEST(BasicStackStringTests, appendBeyondCapacity) {
    BasicStackString<10> string("1234");
    EXPECT_FALSE(string.append("67891011"));
    EXPECT_EQ(string.len(), 4);
    EXPECT_EQ(string.data()[string.len()], '\0');
    EXPECT_EQ(string.getStringView(), "1234");
}

TEST(BasicStackStringTests, operatorPlusEqual) {
    BasicStackString<15> string("1234");
    string += "5678";
    EXPECT_EQ(string.len(), 8);
    EXPECT_EQ(string.getStringView(), "12345678");
    EXPECT_EQ(string.data()[string.len()], '\0');
    string += "10";
    EXPECT_EQ(string.len(), 10);
    EXPECT_EQ(string.getStringView(), "1234567810");
    EXPECT_EQ(string.data()[string.len()], '\0');
}

TEST(BasicStackStringTests, length) {
    BasicStackString<15> string("1234");
    EXPECT_EQ(string.length(), 4);
    string += "5678";
    EXPECT_EQ(string.length(), 8);
}
