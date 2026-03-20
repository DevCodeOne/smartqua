#include "drivers/device_types.h"

#include <gtest/gtest.h>
#include <string_view>

// Define the types supported by DeviceValueUnion
using DeviceUnionTypes = ::testing::Types<float, uint16_t, int16_t, bool>;

// Parameterized test fixture for DeviceValueUnion
template <typename T>
class DeviceValueUnionTypedTest : public ::testing::Test {
public:
    DeviceValueUnion unionValue;
};

// Generate the test suite for all possible DeviceValueUnion types
TYPED_TEST_SUITE_P(DeviceValueUnionTypedTest);

// Test setting and getting values for each type
TYPED_TEST_P(DeviceValueUnionTypedTest, SetAndRetrieveValues)
{
    using CurrentType = TypeParam;

    constexpr DeviceValueUnion::Types::AsTuple normalValues = {123.45f, 200, 12345, -1, true};

    constexpr auto compare = []<typename CompareType>(CompareType number, CompareType expectedNumber, bool exact = true) {
        if constexpr (std::is_same_v<CompareType, float>)
        {
            if (exact)
            {
                EXPECT_FLOAT_EQ(number, expectedNumber);
            }
            else
            {
                EXPECT_NEAR(number, expectedNumber, 0.001f);
            }
        } else
        {
            EXPECT_EQ(number, expectedNumber);
        }
    };

    auto setLimitsAndCompare = [this, compare, normalValues]<typename T>(T) {
        // Test minimum, maximum, and a normal value for float
        ASSERT_TRUE(this->unionValue.template set<T>(std::numeric_limits<CurrentType>::lowest()));
        auto minValue = this->unionValue.template get<T>();
        ASSERT_TRUE(minValue.has_value());
        compare(*minValue, std::numeric_limits<T>::lowest());

        ASSERT_TRUE(this->unionValue.template set<T>(std::numeric_limits<CurrentType>::max()));
        auto maxValue = this->unionValue.template get<T>();
        ASSERT_TRUE(maxValue.has_value());
        compare(*maxValue, std::numeric_limits<T>::max());

        ASSERT_TRUE(this->unionValue.template set<CurrentType>(std::get<T>(normalValues)));
        auto normalValue = this->unionValue.template get<T>();
        ASSERT_TRUE(normalValue.has_value());
        compare(*normalValue, std::get<T>(normalValues), false);
    };

    setLimitsAndCompare(CurrentType{});
}

REGISTER_TYPED_TEST_SUITE_P(DeviceValueUnionTypedTest, SetAndRetrieveValues);
INSTANTIATE_TYPED_TEST_SUITE_P(DeviceValueUnionTest, DeviceValueUnionTypedTest, DeviceUnionTypes);

TEST(DeviceValuesTests, GetUnitOfNone)
{
    DeviceValues values = DeviceValues::create_from_unit(DeviceValueUnit::none, 0);
    EXPECT_FALSE(values.getAsUnitAsType<DeviceValueUnit::percentage>());
}

TEST(DeviceValuesTests, GetAsUnitAndSetToUnit)
{
    DeviceValues values;

    // Test temperature values
    EXPECT_TRUE(values.setToUnit(DeviceValueUnit::temperature, 25.5f));
    auto tempValue = values.getAsUnit<float>(DeviceValueUnit::temperature);
    EXPECT_TRUE(tempValue.has_value());
    EXPECT_FLOAT_EQ(tempValue.value(), 25.5f);

    // Test humidity values
    EXPECT_TRUE(values.setToUnit(DeviceValueUnit::humidity, 50.0f));
    auto humValue = values.getAsUnit<float>(DeviceValueUnit::humidity);
    EXPECT_TRUE(humValue.has_value());
    EXPECT_FLOAT_EQ(humValue.value(), 50.0f);

    // Test percentage values
    EXPECT_TRUE(values.setToUnit<uint8_t>(DeviceValueUnit::percentage, uint8_t(75)));
    auto percentValue = values.getAsUnit<float, true>(DeviceValueUnit::percentage);
    EXPECT_TRUE(percentValue.has_value());
    EXPECT_FLOAT_EQ(percentValue.value(), 75);

    // Test voltage values
    EXPECT_TRUE(values.setToUnit(DeviceValueUnit::voltage, 3.3f));
    auto voltValue = values.getAsUnit<float>(DeviceValueUnit::voltage);
    EXPECT_TRUE(voltValue.has_value());
    EXPECT_FLOAT_EQ(voltValue.value(), 3.3f);

    // Test invalid unit conversions
    EXPECT_FALSE(values.setToUnit(DeviceValueUnit::none, 1.0f));
    EXPECT_FALSE(values.getAsUnit<float>(DeviceValueUnit::none).has_value());

    // Test reading wrong unit type
    values.setToUnit(DeviceValueUnit::temperature, 20.0f);
    EXPECT_FALSE(values.getAsUnit<float>(DeviceValueUnit::voltage).has_value());
}

TEST(DeviceValuesTests, SumOperations)
{
    DeviceValues values1{}, values2{};

    // Test sum of same units
    values1.setToUnit(DeviceValueUnit::temperature, 25.5f);
    values2.setToUnit(DeviceValueUnit::temperature, 10.0f);
    auto sum = values1.sum(values2);
    ASSERT_TRUE(sum.has_value());
    auto sumValue = sum->getAsUnitAsType<DeviceValueUnit::temperature>();
    EXPECT_TRUE(sumValue.has_value());
    EXPECT_FLOAT_EQ(sumValue.value(), 35.5f);

    // Test sum of different units
    values1.setToUnit(DeviceValueUnit::humidity, 50.0f);
    values2.setToUnit(DeviceValueUnit::temperature, 20.0f);
    sum = values1.sum(values2);
    ASSERT_FALSE(sum.has_value());
}

TEST(DeviceValuesTests, DifferenceOperations)
{
    DeviceValues values1, values2;

    // Test difference of same units
    values1.setToUnit(DeviceValueUnit::temperature, 25.5f);
    values2.setToUnit(DeviceValueUnit::temperature, 10.0f);
    auto diff = values1.difference(values2);
    ASSERT_TRUE(diff.has_value());
    auto diffValue = diff->getAsUnitAsType<DeviceValueUnit::temperature>();
    EXPECT_TRUE(diffValue.has_value());
    EXPECT_FLOAT_EQ(diffValue.value(), 15.5f);

    // Test difference of different units
    values1.setToUnit(DeviceValueUnit::humidity, 50.0f);
    values2.setToUnit(DeviceValueUnit::temperature, 20.0f);
    diff = values1.difference(values2);
    ASSERT_FALSE(diff.has_value());
}