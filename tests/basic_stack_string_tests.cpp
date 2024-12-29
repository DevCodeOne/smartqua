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


TEST(BasicStackStringTests, appendMultipleTimes) {
    BasicStackString<20> string("12");
    EXPECT_TRUE(string.append("34"));
    EXPECT_EQ(string.len(), 4);
    EXPECT_EQ(string.getStringView(), "1234");
    EXPECT_EQ(string.data()[string.len()], '\0');

    EXPECT_TRUE(string.append("56"));
    EXPECT_EQ(string.len(), 6);
    EXPECT_EQ(string.getStringView(), "123456");
    EXPECT_EQ(string.data()[string.len()], '\0');

    EXPECT_TRUE(string.append("789"));
    EXPECT_EQ(string.len(), 9);
    EXPECT_EQ(string.getStringView(), "123456789");
    EXPECT_EQ(string.data()[string.len()], '\0');

    EXPECT_FALSE(string.append("0123456789")); // Exceeds capacity
    EXPECT_EQ(string.len(), 9); // Length should remain unchanged
    EXPECT_EQ(string.getStringView(), "123456789");
    EXPECT_EQ(string.data()[string.len()], '\0');
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

TEST(BasicStackStringTests, setStringLiteralWithinCapacity) {
    BasicStackString<10> string;
    EXPECT_TRUE(string.set("12345"));
    EXPECT_EQ(string.len(), 5);
    EXPECT_EQ(string.getStringView(), "12345");
    EXPECT_EQ(string.data()[string.len()], '\0');
}

TEST(BasicStackStringTests, setStringLiteralBeyondCapacity) {
    BasicStackString<5> string;
    EXPECT_FALSE(string.set("123456"));
}

TEST(BasicStackStringTests, setStringLiteralWithLength) {
    BasicStackString<10> string;
    EXPECT_TRUE(string.set("1234567890", 5));
    EXPECT_EQ(string.len(), 5);
    EXPECT_EQ(string.getStringView(), "12345");
    EXPECT_EQ(string.data()[string.len()], '\0');
}

TEST(BasicStackStringTests, setStringLiteralWithLengthBeyondCapacity) {
    BasicStackString<5> string;
    EXPECT_FALSE(string.set("1234567890", 6));
}

TEST(BasicStackStringTests, setStringView) {
    BasicStackString<10> string;
    std::string_view testView("12345");
    EXPECT_TRUE(string.set(testView));
    EXPECT_EQ(string.len(), 5);
    EXPECT_EQ(string.getStringView(), "12345");
    EXPECT_EQ(string.data()[string.len()], '\0');
}

TEST(BasicStackStringTests, setStringViewBeyondCapacity) {
    BasicStackString<5> string;
    std::string_view testView("123456");
    EXPECT_FALSE(string.set(testView));
}

TEST(BasicStackStringTests, setAnotherBasicStackString) {
    BasicStackString<10> string;
    BasicStackString<5> otherString("1234");
    EXPECT_TRUE(string.set(otherString));
    EXPECT_EQ(string.len(), 4);
    EXPECT_EQ(string.getStringView(), "1234");
    EXPECT_EQ(string.data()[string.len()], '\0');
}


TEST(BasicStackStringTests, safeStrLen) {
    const char *validStr = "Hello, World!";
    EXPECT_EQ(safe_strlen(validStr, 255), 13);

    const char *emptyStr = "";
    EXPECT_EQ(safe_strlen(emptyStr, 255), 0);

    const char *nullStr = nullptr;
    EXPECT_EQ(safe_strlen(nullStr, 255), 0);

    EXPECT_EQ(safe_strlen("Hello, World!", 10), 10);
}
