#include <gtest/gtest.h>

#include "utils/check_assign.h"

TEST(CanBeExpressedInTypeTests, SignedIntegers) {
    // Test int to short
    EXPECT_TRUE(canBeExpressedInType<short>(0));                // In range
    EXPECT_TRUE(canBeExpressedInType<short>(std::numeric_limits<short>::max())); // Max short
    EXPECT_TRUE(canBeExpressedInType<short>(std::numeric_limits<short>::min())); // Min short
    EXPECT_FALSE(canBeExpressedInType<short>(std::numeric_limits<int>::max()));  // Out of range
    EXPECT_FALSE(canBeExpressedInType<short>(std::numeric_limits<int>::min()));  // Out of range

    // Test int to char
    EXPECT_TRUE(canBeExpressedInType<char>(0));                // In range
    EXPECT_TRUE(canBeExpressedInType<char>(127));              // Max char
    EXPECT_TRUE(canBeExpressedInType<char>(-128));             // Min char
    EXPECT_FALSE(canBeExpressedInType<char>(128));             // Out of range
    EXPECT_FALSE(canBeExpressedInType<char>(-129));            // Out of range

    // Test long to int
    EXPECT_TRUE(canBeExpressedInType<int>(0L));                // In range
    EXPECT_TRUE(canBeExpressedInType<int>(std::numeric_limits<int>::max()));   // Max int
    EXPECT_TRUE(canBeExpressedInType<int>(std::numeric_limits<int>::min()));   // Min int
    EXPECT_FALSE(canBeExpressedInType<int>(std::numeric_limits<long>::max())); // Out of range
    EXPECT_FALSE(canBeExpressedInType<int>(std::numeric_limits<long>::min())); // Out of range
}

TEST(CanBeExpressedInTypeTests, UnsignedIntegers) {
    // Test int to unsigned int
    EXPECT_TRUE(canBeExpressedInType<unsigned int>(0));                 // In range
    EXPECT_TRUE(canBeExpressedInType<unsigned int>(std::numeric_limits<unsigned int>::max())); // Max unsigned int
    EXPECT_FALSE(canBeExpressedInType<unsigned int>(-1));               // Negative values cannot be expressed
    EXPECT_FALSE(canBeExpressedInType<unsigned int>(std::numeric_limits<long>::max()));        // Out of range

    // Test int to unsigned short
    EXPECT_TRUE(canBeExpressedInType<unsigned short>(0));               // In range
    EXPECT_TRUE(canBeExpressedInType<unsigned short>(std::numeric_limits<unsigned short>::max())); // Max ushort
    EXPECT_FALSE(canBeExpressedInType<unsigned short>(-1));             // Negative values cannot be expressed
    EXPECT_FALSE(canBeExpressedInType<unsigned short>(std::numeric_limits<int>::max()));         // Out of range
}

TEST(CanBeExpressedInTypeTests, FloatingPointToInteger) {
    // Test double to int
    EXPECT_TRUE(canBeExpressedInType<int>(10.0));                      // Exact integer
    EXPECT_TRUE(canBeExpressedInType<int>(-10.0));                     // Exact negative integer
    EXPECT_FALSE(canBeExpressedInType<int>(10.5));                     // Non-integer value
    EXPECT_FALSE(canBeExpressedInType<int>(std::numeric_limits<double>::max())); // Out of range
    EXPECT_FALSE(canBeExpressedInType<int>(std::numeric_limits<double>::lowest())); // Out of range

    // Test float to short
    EXPECT_TRUE(canBeExpressedInType<short>(10.0f));                   // Exact value
    EXPECT_FALSE(canBeExpressedInType<short>(10.5f));                  // Non-integer value
    EXPECT_FALSE(canBeExpressedInType<short>(1e6f));                   // Out of range
}

// Test for isInRangeNoCheck
TEST(IsInRangeNoCheckTests, ValidRange) {
    EXPECT_TRUE(isInRangeNoCheck(5, 1, 10));  // Value within range
    EXPECT_FALSE(isInRangeNoCheck(0, 1, 10)); // Value below range
    EXPECT_FALSE(isInRangeNoCheck(11, 1, 10)); // Value above range

    EXPECT_TRUE(isInRangeNoCheck(1, 1, 10)); // Value equal to the lower bound
    EXPECT_TRUE(isInRangeNoCheck(10, 1, 10)); // Value equal to the upper bound
}

TEST(IsInRangeNoCheckTests, NegativeRange) {
    // Test negative ranges
    EXPECT_TRUE(isInRangeNoCheck(-5, -10, -1)); // Value within negative range
    EXPECT_FALSE(isInRangeNoCheck(-11, -10, -1)); // Value below the range
    EXPECT_FALSE(isInRangeNoCheck(0, -10, -1)); // Value above the range
}

TEST(IsInRangeNoCheckTests, DegenerateRange) {
    // Degenerate range (min == max)
    EXPECT_TRUE(isInRangeNoCheck(5, 5, 5)); // Value equals the range limits
    EXPECT_FALSE(isInRangeNoCheck(4, 5, 5)); // Below limit
    EXPECT_FALSE(isInRangeNoCheck(6, 5, 5)); // Above limit
}

// Test for isInRange
TEST(IsInRangeTests, ValidRange) {
    EXPECT_TRUE(isInRange(5, 1, 10));  // Value within range
    EXPECT_FALSE(isInRange(0, 1, 10)); // Value below range
    EXPECT_FALSE(isInRange(11, 1, 10)); // Value above range

    EXPECT_TRUE(isInRange(1, 1, 10)); // Value equal to the lower bound
    EXPECT_TRUE(isInRange(10, 1, 10)); // Value equal to the upper bound
}

TEST(IsInRangeTests, NegativeRange) {
    // Test negative ranges
    EXPECT_TRUE(isInRange(-5, -10, -1)); // Value within negative range
    EXPECT_FALSE(isInRange(-11, -10, -1)); // Value below the range
    EXPECT_FALSE(isInRange(0, -10, -1)); // Value above the range
}

TEST(IsInRangeTests, DegenerateRange) {
    // Degenerate range (min == max)
    EXPECT_TRUE(isInRange(5, 5, 5)); // Value equals the range limits
    EXPECT_FALSE(isInRange(4, 5, 5)); // Below limit
    EXPECT_FALSE(isInRange(6, 5, 5)); // Above limit
}

// Test for isWithinLimitsForType
TEST(IsWithinLimitsForTypeTests, ValueWithinLimits) {
    EXPECT_TRUE(isWithinLimitsForType<int>(42)); // Regular value
    EXPECT_TRUE(isWithinLimitsForType<int>(std::numeric_limits<int>::min())); // Lower limit
    EXPECT_TRUE(isWithinLimitsForType<int>(std::numeric_limits<int>::max())); // Upper limit
}

TEST(IsWithinLimitsForTypeTests, ValueOutOfLimits) {
    EXPECT_FALSE(isWithinLimitsForType<int>(std::numeric_limits<int>::max() + 1L)); // Just above int max
    EXPECT_FALSE(isWithinLimitsForType<int>(std::numeric_limits<int>::min() - 1L)); // Just below int min
}

// Test for checkAssign
TEST(CheckAssignTests, SuccessfulAssignment) {
    int dest;
    EXPECT_TRUE((checkAssign<int, int>(dest, 42))); // Assigning in-range value
    EXPECT_EQ(dest, 42); // Check the assigned value

    uint8_t dest8;
    EXPECT_TRUE((checkAssign<uint8_t, int>(dest8, 100))); // Assigning in-range value
    EXPECT_EQ(dest8, 100); // Check the assigned value
}

TEST(CheckAssignTests, FailedAssignment) {
    uint8_t dest8;

    // Value too large for uint8_t (256)
    EXPECT_FALSE((checkAssign<uint8_t, int>(dest8, 256)));
    EXPECT_NE(dest8, 256); // Value should not change

    // Negative value for unsigned type
    EXPECT_FALSE((checkAssign<uint8_t, int>(dest8, -1)));
    EXPECT_NE(dest8, -1); // Value should not change
}

TEST(CheckAssignTests, EdgeCases) {
    int dest;

    // Assign minimum and maximum values
    EXPECT_TRUE((checkAssign<int, int>(dest, std::numeric_limits<int>::min())));
    EXPECT_EQ(dest, std::numeric_limits<int>::min());

    EXPECT_TRUE((checkAssign<int, int>(dest, std::numeric_limits<int>::max())));
    EXPECT_EQ(dest, std::numeric_limits<int>::max());
}

// Tests when IsContainedInType is satisfied
TEST(CheckAssignTests, Conversions) {
    // Basic test cases
    EXPECT_TRUE((IsContainedInType<int, int>)) << "int-to-int compatibility should pass";
    EXPECT_TRUE((IsContainedInType<long, int>)) << "long-to-int compatibility should pass";
    EXPECT_TRUE((IsContainedInType<long, char>)) << "char-to-long compatibility should pass";
    EXPECT_TRUE((IsContainedInType<char, char>)) << "char-to-char compatibility should pass";
    EXPECT_TRUE((IsContainedInType<int, uint8_t>)) << "uint8_t-to-int compatibility should pass";
    EXPECT_TRUE((IsContainedInType<int, short>)) << "short-to-int compatibility should pass";

    EXPECT_FALSE((IsContainedInType<int, double>)) << "int-to-double compatibility should not pass";
    EXPECT_FALSE((IsContainedInType<double, int>)) << "double-to-int compatibility should not pass";
    EXPECT_FALSE((IsContainedInType<int, long>)) << "int-to-long compatibility should not pass";
    EXPECT_FALSE((IsContainedInType<char, int>)) << "char-to-int compatibility should not pass";
}