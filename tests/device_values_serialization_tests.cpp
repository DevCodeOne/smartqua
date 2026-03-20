#include "utils/serialization/device_values_serialization.h"
#include "utils/serialization/json_utils.h"

#include <gtest/gtest.h>

// Unit test for `read_from_json<DeviceValueUnit>::read`
TEST(DeviceValueUnitTests, ReadFromJsonParsesValidUnitStringsCorrectly) {
    struct TestCase {
        std::string inputString;
        DeviceValueUnit expectedUnit;
    };

    // Create test cases for each string and its corresponding DeviceValueUnit
    const std::vector<TestCase> testCases = {
        // Percentage
        {"percentage", DeviceValueUnit::percentage},
        {"%", DeviceValueUnit::percentage},

        // Temperature
        {"degc", DeviceValueUnit::temperature},
        {"c", DeviceValueUnit::temperature},
        {"celsius", DeviceValueUnit::temperature},

        // Milliliter
        {"milliliter", DeviceValueUnit::milliliter},
        {"ml", DeviceValueUnit::milliliter},

        // Milligrams
        {"milligrams", DeviceValueUnit::milligrams},
        {"mg", DeviceValueUnit::milligrams},

        // pH
        {"ph", DeviceValueUnit::ph},

        // Humidity
        {"humidity", DeviceValueUnit::humidity},

        // Voltage
        {"voltage", DeviceValueUnit::voltage},
        {"v", DeviceValueUnit::voltage},
        {"volt", DeviceValueUnit::voltage},

        // Ampere
        {"ampere", DeviceValueUnit::ampere},
        {"a", DeviceValueUnit::ampere},
        {"amp", DeviceValueUnit::ampere},

        // TDS
        {"tds", DeviceValueUnit::tds},

        // Generic Analog
        {"analog", DeviceValueUnit::generic_analog},
        {"generic_analog", DeviceValueUnit::generic_analog},

        // Enable
        {"enable", DeviceValueUnit::enable},
        {"bool", DeviceValueUnit::enable},
        {"switch", DeviceValueUnit::enable},

        // Seconds
        {"s", DeviceValueUnit::seconds},
        {"sec", DeviceValueUnit::seconds},

        // PWM
        {"pwm", DeviceValueUnit::generic_pwm},

        // Invalid cases (should map to DeviceValueUnit::none)
        {"invalid", DeviceValueUnit::none},
        {"", DeviceValueUnit::none},
        {"random_string", DeviceValueUnit::none}
    };

    // Iterate through each test case
    for (const auto& testCase : testCases) {
        // Perform the test
        DeviceValueUnit result = DeviceValueUnit::none;
        read_from_json<DeviceValueUnit>::read(testCase.inputString.c_str(), static_cast<int>(testCase.inputString.length()), result);

        // Assert the result matches the expected unit
        EXPECT_EQ(result, testCase.expectedUnit) << "Failed for input string: " << testCase.inputString;
    }
}

// Helper function to test JSON deserialization for a specific unit.
template <typename T>
void test_device_values_deserialization(DeviceValueUnit unit, const std::string& json, const T& expected_value) {
    DeviceValues values;
    read_from_json<DeviceValues>::read(json.c_str(), json.length(), values);

    ASSERT_EQ(values.getUnit(), unit) << "Incorrect unit deserialized.";
    auto result = values.getAsUnit<T>();
    ASSERT_TRUE(result.has_value()) << "Deserialized value is not set.";
    ASSERT_EQ(result.value(), expected_value) << "Deserialized value does not match the expected value.";
}

TEST(DeviceValueUnitTests, DeserializeTemperature) {
    test_device_values_deserialization<float>(DeviceValueUnit::temperature, R"({ "temperature": 25.5 })", 25.5f);
}

TEST(DeviceValueUnitTests, DeserializePh) {
    test_device_values_deserialization<float>(DeviceValueUnit::ph, R"({ "ph": 7.2 })", 7.2f);
}

TEST(DeviceValueUnitTests, DeserializeHumidity) {
    test_device_values_deserialization<float>(DeviceValueUnit::humidity, R"({ "humidity": 55.5 })", 55.5f);
}

TEST(DeviceValueUnitTests, DeserializeVoltage) {
    test_device_values_deserialization<float>(DeviceValueUnit::voltage, R"({ "voltage": 3.3 })", 3.3f);
}

TEST(DeviceValueUnitTests, DeserializeAmpere) {
    test_device_values_deserialization<float>(DeviceValueUnit::ampere, R"({ "ampere": 1.5 })", 1.5f);
}

TEST(DeviceValueUnitTests, DeserializeWatt) {
    test_device_values_deserialization<float>(DeviceValueUnit::watt, R"({ "watt": 5.0 })", 5.0f);
}

TEST(DeviceValueUnitTests, DeserializeTds) {
    test_device_values_deserialization<uint16_t>(DeviceValueUnit::tds, R"({ "tds": 500 })", 500);
}

TEST(DeviceValueUnitTests, DeserializeGenericAnalog) {
    test_device_values_deserialization<uint16_t>(DeviceValueUnit::generic_analog, R"({ "generic_analog": 1023 })", 1023);
}

TEST(DeviceValueUnitTests, DeserializeGenericPwm) {
    test_device_values_deserialization<uint16_t>(DeviceValueUnit::generic_pwm, R"({ "generic_pwm": 128 })", 128);
}

TEST(DeviceValueUnitTests, DeserializeMilligrams) {
    test_device_values_deserialization<int16_t>(DeviceValueUnit::milligrams, R"({ "milligrams": -50 })", -50);
}

TEST(DeviceValueUnitTests, DeserializeMilliliter) {
    test_device_values_deserialization<float>(DeviceValueUnit::milliliter, R"({ "milliliter": 15.75 })", 15.75f);
}

TEST(DeviceValueUnitTests, DeserializeEnable) {
    test_device_values_deserialization<bool>(DeviceValueUnit::enable, R"({ "enable": true })", true);
    test_device_values_deserialization<bool>(DeviceValueUnit::enable, R"({ "enable": false })", false);
}

TEST(DeviceValueUnitTests, DeserializePercentage) {
    test_device_values_deserialization<uint8_t>(DeviceValueUnit::percentage, R"({ "percentage": 75 })", 75);
}

TEST(DeviceValueUnitTests, DeserializeSeconds) {
    test_device_values_deserialization<uint16_t>(DeviceValueUnit::seconds, R"({ "seconds": 3600 })", 3600);
}

TEST(DeviceValueUnitTests, DeserializeGenericUnsignedIntegral) {
    test_device_values_deserialization<uint16_t>(DeviceValueUnit::generic_unsigned_integral, R"({ "generic_unsigned_integral": 250 })", 250);
}

TEST(DeviceValueUnitTests, DeserializeUnknownUnit) {
    DeviceValues values;
    read_from_json<DeviceValues>::read(R"({ "unknown": 123 })", 18, values);

    ASSERT_EQ(values.getUnit(), DeviceValueUnit::none) << "Unit should be set to 'none' for unknown input.";
}

// Helper function to test JSON serialization for a specific unit
template <typename T>
void test_device_values_serialization(DeviceValueUnit unit, const T& value, const std::string& expected_json)
{
    DeviceValues values;
    values.setToUnit(unit, value);

    char buffer[256];
    json_out jsonOut = JSON_OUT_BUF(buffer, 256);
    print_to_json<DeviceValues>::print(&jsonOut, values);

    EXPECT_STREQ(buffer, expected_json.c_str()) << "Serialized JSON does not match expected output.";
}

TEST(DeviceValueUnitTests, SerializeTemperature)
{
    test_device_values_serialization<float>(DeviceValueUnit::temperature, 25.5f, R"({"temperature":25.50000})");
}

TEST(DeviceValueUnitTests, SerializePh)
{
    test_device_values_serialization<float>(DeviceValueUnit::ph, 7.2f, R"({"ph":7.20000})");
}

TEST(DeviceValueUnitTests, SerializeHumidity)
{
    test_device_values_serialization<float>(DeviceValueUnit::humidity, 55.5f, R"({"humidity":55.50000})");
}

TEST(DeviceValueUnitTests, SerializeVoltage)
{
    test_device_values_serialization<float>(DeviceValueUnit::voltage, 3.3f, R"({"voltage":3.30000})");
}

TEST(DeviceValueUnitTests, SerializeAmpere)
{
    test_device_values_serialization<float>(DeviceValueUnit::ampere, 1.5f, R"({"ampere":1.50000})");
}

TEST(DeviceValueUnitTests, SerializeWatt)
{
    test_device_values_serialization<float>(DeviceValueUnit::watt, 5.0f, R"({"watt":5.00000})");
}

TEST(DeviceValueUnitTests, SerializeTds)
{
    test_device_values_serialization<uint16_t>(DeviceValueUnit::tds, 500, R"({"tds":500})");
}

TEST(DeviceValueUnitTests, SerializeGenericAnalog)
{
    test_device_values_serialization<uint16_t>(DeviceValueUnit::generic_analog, 1023, R"({"generic_analog":1023})");
}

TEST(DeviceValueUnitTests, SerializeGenericPwm)
{
    test_device_values_serialization<uint16_t>(DeviceValueUnit::generic_pwm, 128, R"({"generic_pwm":128})");
}

TEST(DeviceValueUnitTests, SerializeMilligrams)
{
    test_device_values_serialization<int16_t>(DeviceValueUnit::milligrams, -50, R"({"milligrams":-50})");
}

TEST(DeviceValueUnitTests, SerializeMilliliter)
{
    test_device_values_serialization<float>(DeviceValueUnit::milliliter, 15.75f, R"({"milliliter":15.75000})");
}

TEST(DeviceValueUnitTests, SerializeEnable)
{
    test_device_values_serialization<bool>(DeviceValueUnit::enable, true, R"({"enable":true})");
    test_device_values_serialization<bool>(DeviceValueUnit::enable, false, R"({"enable":false})");
}

TEST(DeviceValueUnitTests, SerializePercentage)
{
    test_device_values_serialization<uint8_t>(DeviceValueUnit::percentage, 75, R"({"percentage":75})");
}

TEST(DeviceValueUnitTests, SerializeSeconds)
{
    test_device_values_serialization<uint16_t>(DeviceValueUnit::seconds, 3600, R"({"seconds":3600})");
}

TEST(DeviceValueUnitTests, SerializeGenericUnsignedIntegral)
{
    test_device_values_serialization<uint16_t>(DeviceValueUnit::generic_unsigned_integral, 250,
                                               R"({"generic_unsigned_integral":250})");
}

TEST(DeviceValueUnitTests, SerializeNoneUnit)
{
    DeviceValues values = DeviceValues::create_from_unit(DeviceValueUnit::none, 0);
    char buffer[256];
    json_out jsonOut = JSON_OUT_BUF(buffer, 256);
    print_to_json<DeviceValues>::print(&jsonOut, values);
    EXPECT_STREQ(buffer, "{}") << "Empty DeviceValues should serialize to empty JSON object.";
}

