#include <gtest/gtest.h>

#include "utils/container/fixed_size_optional_array.h"

TEST(FixedSizeOptionalArrayTest, DefaultConstructor) {
    const FixedSizeOptionalArray<int, 5> array;
    EXPECT_EQ(array.size(), 5);
    EXPECT_TRUE(array.empty());
}

TEST(FixedSizeOptionalArrayTest, InsertAndAccess) {
    FixedSizeOptionalArray<int, 5> array;
    array.insert(0, 10);
    array.insert(1, 20);
    EXPECT_EQ(array[0].value(), 10);
    EXPECT_EQ(array[1].value(), 20);
    EXPECT_FALSE(array.empty());
}

TEST(FixedSizeOptionalArrayTest, Erase) {
    FixedSizeOptionalArray<int, 5> array;
    array.insert(0, 10);
    array.erase(0);
    EXPECT_FALSE(array[0].has_value());
    EXPECT_TRUE(array.empty());
}

TEST(FixedSizeOptionalArrayTest, Clear) {
    FixedSizeOptionalArray<int, 5> array;
    array.insert(0, 10);
    array.insert(1, 20);
    array.clear();
    EXPECT_TRUE(array.empty());
    EXPECT_FALSE(array[0].has_value());
    EXPECT_FALSE(array[1].has_value());
}

TEST(FixedSizeOptionalArrayTest, Append) {
    FixedSizeOptionalArray<int, 2> array;
    EXPECT_TRUE(array.append(10));
    EXPECT_TRUE(array.append(20));
    EXPECT_EQ(array[0].value(), 10);
    EXPECT_EQ(array[1].value(), 20);
    EXPECT_FALSE(array.append(30));
}

TEST(FixedSizeOptionalArrayTest, Count) {
    FixedSizeOptionalArray<int, 5> array;
    array.insert(0, 10);
    array.insert(1, 20);
    EXPECT_EQ(array.count(), 2);
    array.erase(0);
    EXPECT_EQ(array.count(), 1);
}