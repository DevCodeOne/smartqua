#include <cstdint>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "utils/linear_mapping_utils.h"

TEST(LinearInterpolate, MapsZeroPercentToOutputStart)
{
    const auto result = linear_alg::linearInterpolate<uint16_t>(
        0,
        0,
        100,
        uint16_t{0},
        uint16_t{1023});

    EXPECT_EQ(result, uint16_t{0});
}

TEST(LinearInterpolate, MapsFiftyPercentToMiddleForIntegerOutput)
{
    const auto result = linear_alg::linearInterpolate<uint16_t>(
        50,
        0,
        100,
        uint16_t{0},
        uint16_t{1000});

    EXPECT_EQ(result, uint16_t{500});
}

TEST(LinearInterpolate, MapsHundredPercentToOutputEnd)
{
    const auto result = linear_alg::linearInterpolate<uint16_t>(
        100,
        0,
        100,
        uint16_t{0},
        uint16_t{1023});

    EXPECT_EQ(result, uint16_t{1023});
}

TEST(LinearInterpolate, SupportsFloatingPointOutput)
{
    const auto result = linear_alg::linearInterpolate<float>(
        25.0f,
        0.0f,
        100.0f,
        0.0f,
        10.0f);

    EXPECT_FLOAT_EQ(result, 2.5f);
}

TEST(LinearInterpolate, SupportsMixedInputAndOutputTypes)
{
    const auto result = linear_alg::linearInterpolate<double>(
        50,
        0,
        100,
        1.5f,
        2.5f);

    EXPECT_DOUBLE_EQ(result, 2.0);
}

TEST(LinearInterpolate, SupportsNegativeOutputRange)
{
    const auto result = linear_alg::linearInterpolate<int>(
        50,
        0,
        100,
        -100,
        100);

    EXPECT_EQ(result, 0);
}

TEST(LinearInterpolate, SupportsDescendingInputRange)
{
    const auto result = linear_alg::linearInterpolate<int>(
        75,
        100,
        0,
        0,
        100);

    EXPECT_EQ(result, 25);
}

TEST(LinearInterpolate, ReturnsOutputStartWhenInputRangeHasNoSize)
{
    const auto result = linear_alg::linearInterpolate<int>(
        50,
        100,
        100,
        42,
        100);

    EXPECT_EQ(result, 42);
}

TEST(LinearMap, MapsZeroFiftyAndHundredPercentToPwmRange)
{
    EXPECT_EQ((linear_alg::linearMap<uint16_t>(0, 0, 100, uint16_t{0}, uint16_t{1000})), uint16_t{0});
    EXPECT_EQ((linear_alg::linearMap<uint16_t>(50, 0, 100, uint16_t{0}, uint16_t{1000})), uint16_t{500});
    EXPECT_EQ((linear_alg::linearMap<uint16_t>(100, 0, 100, uint16_t{0}, uint16_t{1000})), uint16_t{1000});
}

TEST(LinearMap, MapsPercentageToUint8MaxValue)
{
    EXPECT_EQ((linear_alg::linearMap<uint8_t>(0, 0, 100, uint8_t{0}, std::numeric_limits<uint8_t>::max())),
              uint8_t{0});
    EXPECT_EQ((linear_alg::linearMap<uint8_t>(100, 0, 100, uint8_t{0}, std::numeric_limits<uint8_t>::max())),
              std::numeric_limits<uint8_t>::max());
}

TEST(LinearMap, ClampsInputBelowRangeByDefault)
{
    const auto result = linear_alg::linearMap<uint16_t>(
        -50,
        0,
        100,
        uint16_t{0},
        uint16_t{1000});

    EXPECT_EQ(result, uint16_t{0});
}

TEST(LinearMap, ClampsInputAboveRangeByDefault)
{
    const auto result = linear_alg::linearMap<uint16_t>(
        150,
        0,
        100,
        uint16_t{0},
        uint16_t{1000});

    EXPECT_EQ(result, uint16_t{1000});
}

TEST(LinearMap, CanDisableClampingBelowRange)
{
    const auto result = linear_alg::linearMap<int>(
        -50,
        0,
        100,
        0,
        1000,
        false);

    EXPECT_EQ(result, -500);
}

TEST(LinearMap, CanDisableClampingAboveRange)
{
    const auto result = linear_alg::linearMap<int>(
        150,
        0,
        100,
        0,
        1000,
        false);

    EXPECT_EQ(result, 1500);
}

TEST(LinearMap, SupportsFloatingPointInputAndOutput)
{
    const auto result = linear_alg::linearMap<float>(
        0.5f,
        0.0f,
        1.0f,
        0.0f,
        3.3f);

    EXPECT_FLOAT_EQ(result, 1.65f);
}

TEST(LinearMap, SupportsDoubleOutput)
{
    const auto result = linear_alg::linearMap<double>(
        75,
        0,
        100,
        0.0,
        1.0);

    EXPECT_DOUBLE_EQ(result, 0.75);
}

TEST(LinearMap, SupportsDescendingInputRangeWithClamping)
{
    EXPECT_EQ((linear_alg::linearMap<int>(100, 100, 0, 0, 100)), 0);
    EXPECT_EQ((linear_alg::linearMap<int>(50, 100, 0, 0, 100)), 50);
    EXPECT_EQ((linear_alg::linearMap<int>(0, 100, 0, 0, 100)), 100);
}

TEST(LinearMap, ClampsDescendingInputRangeBelowLowerBound)
{
    const auto result = linear_alg::linearMap<int>(
        -50,
        100,
        0,
        0,
        100);

    EXPECT_EQ(result, 100);
}

TEST(LinearMap, ClampsDescendingInputRangeAboveUpperBound)
{
    const auto result = linear_alg::linearMap<int>(
        150,
        100,
        0,
        0,
        100);

    EXPECT_EQ(result, 0);
}

TEST(LinearMap, SupportsDescendingOutputRange)
{
    EXPECT_EQ((linear_alg::linearMap<int>(0, 0, 100, 100, 0)), 100);
    EXPECT_EQ((linear_alg::linearMap<int>(50, 0, 100, 100, 0)), 50);
    EXPECT_EQ((linear_alg::linearMap<int>(100, 0, 100, 100, 0)), 0);
}

TEST(LinearMap, ReturnsOutputStartWhenInputRangeHasNoSize)
{
    const auto result = linear_alg::linearMap<int>(
        50,
        100,
        100,
        42,
        100);

    EXPECT_EQ(result, 42);
}

TEST(LinearMap, IsUsableInConstexprContext)
{
    constexpr auto result = linear_alg::linearMap<int>(
        50,
        0,
        100,
        0,
        1000);

    static_assert(result == 500);
    EXPECT_EQ(result, 500);
}

TEST(LinearInterpolate, IsUsableInConstexprContext)
{
    constexpr auto result = linear_alg::linearInterpolate<int>(
        25,
        0,
        100,
        0,
        1000);

    static_assert(result == 250);
    EXPECT_EQ(result, 250);
}
