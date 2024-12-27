#include <gtest/gtest.h>

#include "utils/container/fixed_size_optional_array.h"

// Test class and fixture
class FixedSizeOptionalArrayTest : public ::testing::Test {
protected:
    FixedSizeOptionalArray<int, 5> array; // A test instance with max size 5

    void SetUp() override {
        (void) array.append(10); // Add test data
        (void) array.append(20);
        (void) array.append(30);
    }
};

TEST_F(FixedSizeOptionalArrayTest, DefaultConstructor) {
    const FixedSizeOptionalArray<int, 5> array;
    EXPECT_EQ(array.size(), 5);
    EXPECT_TRUE(array.empty());
}

TEST_F(FixedSizeOptionalArrayTest, InsertAndAccess) {
    EXPECT_EQ(array[0].value(), 10);
    EXPECT_EQ(array[1].value(), 20);
    EXPECT_FALSE(array.empty());
}

TEST_F(FixedSizeOptionalArrayTest, Erase) {
    FixedSizeOptionalArray<int, 5> array;
    array.erase(0);
    EXPECT_FALSE(array[0].has_value());
    EXPECT_TRUE(array.empty());
}

TEST_F(FixedSizeOptionalArrayTest, Clear) {
    array.clear();
    EXPECT_TRUE(array.empty());
    EXPECT_FALSE(array[0].has_value());
    EXPECT_FALSE(array[1].has_value());
}

TEST_F(FixedSizeOptionalArrayTest, Append) {
    FixedSizeOptionalArray<int, 2> array;
    EXPECT_TRUE(array.append(10));
    EXPECT_TRUE(array.append(20));
    EXPECT_EQ(array[0].value(), 10);
    EXPECT_EQ(array[1].value(), 20);
    EXPECT_FALSE(array.append(30));
}

TEST_F(FixedSizeOptionalArrayTest, Count) {
    FixedSizeOptionalArray<int, 5> array;
    array.insert(0, 10);
    array.insert(1, 20);
    EXPECT_EQ(array.count(), 2);
    array.erase(0);
    EXPECT_EQ(array.count(), 1);
}

// Test: contains method should return true for existing values
TEST_F(FixedSizeOptionalArrayTest, ContainsExistingValue) {
    EXPECT_TRUE(array.contains(10)); // Check if 10 exists
    EXPECT_TRUE(array.contains(20)); // Check if 20 exists
    EXPECT_TRUE(array.contains(30)); // Check if 30 exists
}

// Test: contains method should return false for non-existing values
TEST_F(FixedSizeOptionalArrayTest, DoesNotContainNonExistingValue) {
    EXPECT_FALSE(array.contains(40)); // 40 does not exist
    EXPECT_FALSE(array.contains(50)); // 50 does not exist
}

// Test: contains method should return false for an empty array
TEST_F(FixedSizeOptionalArrayTest, DoesNotContainInEmptyArray) {
    FixedSizeOptionalArray<int, 5> emptyArray; // Create an empty array
    EXPECT_FALSE(emptyArray.contains(10)); // Should return false
}

// Test: contains method with duplicate entries can detect the value
TEST_F(FixedSizeOptionalArrayTest, HandlesDuplicateValues) {
    (void) array.append(20); // Add a duplicate value
    EXPECT_TRUE(array.contains(20)); // Should still return true
}

// Test class and fixture
class FixedSizeOptionalArrayRemoveAllOccurrencesTest : public ::testing::Test {
protected:
    FixedSizeOptionalArray<int, 5> array; // Array with fixed size of 5

    void SetUp() override {
        (void) array.append(10);
        (void) array.append(20);
        (void) array.append(30);
        (void) array.append(20); // Duplicate value
    }
};

TEST_F(FixedSizeOptionalArrayRemoveAllOccurrencesTest, RemovesAllOccurrencesOfValue) {
    EXPECT_TRUE(array.removeValue(20));   // Remove 20
    EXPECT_FALSE(array.contains(20));    // Ensure no 20 exists in the array
    EXPECT_EQ(array.count(), 2);         // Remaining count should be 2
}

// Test: removeValue does nothing for non-existing values
TEST_F(FixedSizeOptionalArrayRemoveAllOccurrencesTest, DoesNotRemoveNonExistingValue) {
    EXPECT_FALSE(array.removeValue(40)); // Value 40 does not exist
    EXPECT_EQ(array.count(), 4);         // Count should remain the same
}

// Test: removeValue on an array with only duplicates removes all of them
TEST_F(FixedSizeOptionalArrayRemoveAllOccurrencesTest, RemovesOnlyDuplicates) {
    array.clear(); // Clear all elements
    (void) array.append(20);
    (void) array.append(20);
    (void) array.append(20);

    EXPECT_TRUE(array.removeValue(20));  // Remove all instances of 20
    EXPECT_FALSE(array.contains(20));   // No 20s should remain
    EXPECT_EQ(array.count(), 0);        // Count should be 0
}

// Test: removeValue does nothing for an empty array
TEST_F(FixedSizeOptionalArrayRemoveAllOccurrencesTest, RemoveValueFromEmptyArray) {
    array.clear();                      // Clear all elements
    EXPECT_FALSE(array.removeValue(10)); // Attempt to remove from empty array
    EXPECT_TRUE(array.empty());         // Ensure array is still empty
}

// Test: removeValue removes the only item in a single-element array
TEST_F(FixedSizeOptionalArrayRemoveAllOccurrencesTest, RemovesSingleValueCorrectly) {
    FixedSizeOptionalArray<int, 1> singleValueArray;
    (void) singleValueArray.append(10);       // Add a single value to the array
    EXPECT_TRUE(singleValueArray.removeValue(10)); // Remove the only value
    EXPECT_FALSE(singleValueArray.contains(10));   // Ensure it's removed
    EXPECT_TRUE(singleValueArray.empty());         // Array should now be empty
}

// Test: removeValue updates the array's internal state correctly
TEST_F(FixedSizeOptionalArrayRemoveAllOccurrencesTest, UpdatesArrayStateAfterRemoval) {
    EXPECT_TRUE(array.removeValue(30)); // Remove value 30
    EXPECT_FALSE(array.contains(30));  // Ensure 30 is removed
    EXPECT_EQ(array.count(), 3);       // Count should decrease correctly
}

TEST_F(FixedSizeOptionalArrayTest, IterateNonEmptyElements) {
    std::vector<int> values;
    for (auto it = array.begin(); it != array.end(); ++it) {
        values.push_back(*it);
    }

    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 30);
}

TEST_F(FixedSizeOptionalArrayTest, EmptyArray) {
    array.clear();

    std::vector<int> values;
    for (auto it = array.begin(); it != array.end(); ++it) {
        values.push_back(*it);
    }

    EXPECT_EQ(values.size(), 0);
}

TEST_F(FixedSizeOptionalArrayTest, IterateAndModify) {
    array.clear();
    array.insert(1, 15);
    array.insert(3, 25);

    for (auto it = array.begin(); it != array.end(); ++it) {
        *it += 5;
    }

    EXPECT_EQ(array[1], 20);
    EXPECT_EQ(array[3], 30);
}
