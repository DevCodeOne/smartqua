#include <gtest/gtest.h>

#include <string_view>

#include "utils/serialization/number_conversion.h"

namespace
{
struct ConstCharRange
{
    const char *first;
    const char *last;

    [[nodiscard]] const char *begin() const
    {
        return first;
    }

    [[nodiscard]] const char *end() const
    {
        return last;
    }
};

ConstCharRange makeRange(const char *value)
{
    return ConstCharRange{value, value + std::char_traits<char>::length(value)};
}
}

TEST(ConvertFromCharRange, ConvertsSignedIntegerFromStringView)
{
    const std::string_view input{"-42"};

    const auto result = convertFromCharRange<int>(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -42);
}

TEST(ConvertFromCharRange, ConvertsUnsignedIntegerFromStringView)
{
    const std::string_view input{"42"};

    const auto result = convertFromCharRange<unsigned int>(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42U);
}

TEST(ConvertFromCharRange, RejectsNegativeValueForUnsignedInteger)
{
    const std::string_view input{"-42"};

    const auto result = convertFromCharRange<unsigned int>(input);

    EXPECT_FALSE(result.has_value());
}

TEST(ConvertFromCharRange, ConvertsLongLongFromCustomCharRange)
{
    const auto input = makeRange("-922337203685477580");

    const auto result = convertFromCharRange<long long>(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -922337203685477580LL);
}

TEST(ConvertFromCharRange, ConvertsUnsignedLongLongFromCustomCharRange)
{
    const auto input = makeRange("1844674407370955161");

    const auto result = convertFromCharRange<unsigned long long>(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1844674407370955161ULL);
}

TEST(ConvertFromCharRange, ConvertsFloatFromStringView)
{
    const std::string_view input{"3.5"};

    const auto result = convertFromCharRange<float>(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(*result, 3.5F);
}

TEST(ConvertFromCharRange, ConvertsNegativeDoubleFromCustomCharRange)
{
    const auto input = makeRange("-123.456");

    const auto result = convertFromCharRange<double>(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, -123.456);
}

TEST(ConvertFromCharRange, RejectsInvalidIntegerInput)
{
    const std::string_view input{"abc"};

    const auto result = convertFromCharRange<int>(input);

    EXPECT_FALSE(result.has_value());
}

TEST(ConvertFromCharRange, RejectsOutOfRangeIntegerInput)
{
    const std::string_view input{"999999999999999999999999999999999999999999999999"};

    const auto result = convertFromCharRange<int>(input);

    EXPECT_FALSE(result.has_value());
}

TEST(ConvertFromCharRange, AcceptsPartiallyParsedInputWhenFromCharsSucceeds)
{
    const std::string_view input{"123abc"};

    const auto result = convertFromCharRange<int>(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}