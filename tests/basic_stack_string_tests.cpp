#include "utils/stack_string.h"

#include <gtest/gtest.h>

TEST(BasicStackStringTests, initializeWithNullptr)
{
    // Verifying that initialization with nullptr works correctly.
    BasicStackString<10> string;
    EXPECT_FALSE(string.set(nullptr));
    EXPECT_EQ(string.len(), 0);
    EXPECT_EQ(string.capacity(), 10);
    EXPECT_TRUE(string.empty());
}

TEST(BasicStackStringTests, assignNullptr)
{
    // Verifying assignment using nullptr does not overwrite the current contents.
    BasicStackString<10> string("1234");
    EXPECT_FALSE(string.set(nullptr));
    EXPECT_EQ(string.len(), 4);
    EXPECT_EQ(string.getStringView(), "1234");
}

TEST(BasicStackStringTests, appendNullptr)
{
    // Verifying that appending nullptr does not modify the string.
    BasicStackString<10> string("1234");
    EXPECT_FALSE(string.append(nullptr));
    EXPECT_EQ(string.len(), 4);
    EXPECT_EQ(string.getStringView(), "1234");
}

TEST(BasicStackStringTests, addAssignOperatorNullptr)
{
    // Verifying that += operator with nullptr does not alter the string.
    BasicStackString<10> string("1234");
    string += nullptr;
    EXPECT_EQ(string.len(), 4);
    EXPECT_EQ(string.getStringView(), "1234");
}

TEST(BasicStackStringTests, equalityComparisonWithNullptr)
{
    // Verifying comparison between a string and nullptr.
    BasicStackString<10> string("1234");

    EXPECT_FALSE(string == nullptr);
    EXPECT_TRUE(BasicStackString<10>{} == nullptr); // Empty string equals nullptr
}

TEST(BasicStackStringTests, safeStrlenWithNullptr)
{
    // Re-validating that safeStrLen returns correctly for nullptr inputs.
    EXPECT_EQ(safeStrLen(static_cast<const char*>(nullptr), 100), 0);
    EXPECT_EQ(safeStrLen(static_cast<const char*>(nullptr), 0), 0);
}

TEST(BasicStackStringTests, setUsingNullptrAndLength) {
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
    EXPECT_EQ(safeStrLen(validStr, 255), 13);

    const char *emptyStr = "";
    EXPECT_EQ(safeStrLen(emptyStr, 255), 0);

    const char *nullStr = nullptr;
    EXPECT_EQ(safeStrLen(nullStr, 255), 0);

    EXPECT_EQ(safeStrLen("Hello, World!", 10), 10);
}

TEST(BasicStackStringTests, equalOperator) {
    BasicStackString<10> string1("12345");
    BasicStackString<10> string2("12345");
    BasicStackString<10> string3("54321");
    BasicStackString<5> string4("123");

    // Test equality of two strings with identical content
    EXPECT_TRUE(string1 == string2);

    // Test inequality of two strings with different content
    EXPECT_FALSE(string1 == string3);

    // Test inequality of two strings with different sizes
    EXPECT_FALSE(string1 == string4);

    // Test self-equality
    EXPECT_TRUE(string1 == string1);

    // Test empty string equality
    BasicStackString<10> emptyString1{""};
    BasicStackString<10> emptyString2{""};
    EXPECT_TRUE(emptyString1 == emptyString2);

    // Test inequality of empty string and a non-empty string
    EXPECT_FALSE(emptyString1 == string1);
}


TEST(BasicStackStringTests, stringViewEqualityOperator) {
    BasicStackString<5> string("1234");
    std::string_view shortView("1234");
    std::string_view longView("123456789");

    // Test equality of string with a matching string_view
    EXPECT_TRUE(string == shortView);

    // Test inequality of string with a longer string_view
    EXPECT_FALSE(string == longView);

    // Test inequality when string_view is empty
    std::string_view emptyView("");
    EXPECT_FALSE(string == emptyView);

    // Test inequality of an empty BasicStackString with a non-empty string_view
    BasicStackString<5> emptyString{""};
    EXPECT_FALSE(emptyString == shortView);

    // Test equality of an empty BasicStackString with an empty string_view
    EXPECT_TRUE(emptyString == emptyView);
}
