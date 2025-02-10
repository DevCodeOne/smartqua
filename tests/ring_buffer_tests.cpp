#include "utils/container/ring_buffer.h"
#include "utils/container/sample_container.h"
#include "utils/container/lookup_table.h"

#include <gtest/gtest.h>

TEST(RingBuffer, Basic) {
    RingBuffer<int, 10> rb;
    for (size_t i = 0; i < 10; ++i) {
        rb.append(static_cast<int>(i));
    }

    EXPECT_EQ(rb.size(), 10);
    EXPECT_EQ(rb.front(), 0);
    EXPECT_EQ(rb.back(), 9);

    rb.append(10);
    EXPECT_EQ(rb.size(), 10);
    EXPECT_EQ(rb.front(), 1);
    EXPECT_EQ(rb.back(), 10);

    rb.append(11);
    EXPECT_EQ(rb.size(), 10);
    EXPECT_EQ(rb.front(), 2);
    EXPECT_EQ(rb.back(), 11);
}

#include <gtest/gtest.h>
#include "utils/container/ring_buffer.h"

TEST(RingBuffer, RemoveFront) {
    RingBuffer<int, 5> rb;

    // Test removing from an empty buffer
    rb.removeFront();
    EXPECT_TRUE(rb.empty());
    EXPECT_EQ(rb.size(), 0);

    // Fill the buffer
    for (int i = 1; i <= 5; ++i) {
        rb.append(i);
    }
    EXPECT_EQ(rb.size(), 5);
    EXPECT_EQ(rb.front(), 1);

    // Remove elements from the front
    rb.removeFront();
    EXPECT_EQ(rb.size(), 4);
    EXPECT_EQ(rb.front(), 2);

    rb.removeFront();
    EXPECT_EQ(rb.size(), 3);
    EXPECT_EQ(rb.front(), 3);

    // Remove all remaining elements
    rb.removeFront();
    rb.removeFront();
    rb.removeFront();
    EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, RemoveBack) {
    RingBuffer<int, 5> rb;

    // Test removing from an empty buffer
    rb.removeBack();
    EXPECT_TRUE(rb.empty());
    EXPECT_EQ(rb.size(), 0);

    // Fill the buffer
    for (int i = 1; i <= 5; ++i) {
        rb.append(i);
    }
    EXPECT_EQ(rb.size(), 5);
    EXPECT_EQ(rb.back(), 5);

    // Remove elements from the back
    rb.removeBack();
    EXPECT_EQ(rb.size(), 4);
    EXPECT_EQ(rb.back(), 4);

    rb.removeBack();
    EXPECT_EQ(rb.size(), 3);
    EXPECT_EQ(rb.back(), 3);

    // Remove all remaining elements
    rb.removeBack();
    rb.removeBack();
    rb.removeBack();
    EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, TakeBack) {
    RingBuffer<int, 5> rb;

    // Test taking from an empty buffer
    auto value = rb.takeBack();
    EXPECT_FALSE(value.has_value());

    // Fill the buffer
    for (int i = 1; i <= 5; ++i) {
        rb.append(i);
    }
    EXPECT_EQ(rb.size(), 5);
    EXPECT_EQ(rb.back(), 5);

    // Take elements from the back
    value = rb.takeBack();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 5);
    EXPECT_EQ(rb.size(), 4);

    value = rb.takeBack();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 4);
    EXPECT_EQ(rb.size(), 3);

    // Take all remaining elements
    rb.takeBack();
    rb.takeBack();
    value = rb.takeBack();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
    EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, TakeFront) {
    RingBuffer<int, 5> rb;

    // Test taking from an empty buffer
    auto value = rb.takeFront();
    EXPECT_FALSE(value.has_value());

    // Fill the buffer
    for (int i = 1; i <= 5; ++i) {
        rb.append(i);
    }
    EXPECT_EQ(rb.size(), 5);
    EXPECT_EQ(rb.front(), 1);

    // Take elements from the front
    value = rb.takeFront();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
    EXPECT_EQ(rb.size(), 4);

    value = rb.takeFront();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 2);
    EXPECT_EQ(rb.size(), 3);

    // Take all remaining elements
    rb.takeFront();
    rb.takeFront();
    value = rb.takeFront();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 5);
    EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, MixedOperations) {
    RingBuffer<int, 5> rb;

    // Mixed operations
    rb.append(1);
    rb.append(2);
    rb.append(3);

    EXPECT_EQ(rb.size(), 3);
    EXPECT_EQ(rb.front(), 1);
    EXPECT_EQ(rb.back(), 3);

    // TakeFront and RemoveBack
    auto value = rb.takeFront();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
    EXPECT_EQ(rb.size(), 2);

    rb.removeBack();
    EXPECT_EQ(rb.size(), 1);
    EXPECT_EQ(rb.front(), 2);

    // Add more elements and test wrap-around behavior
    rb.append(4);
    rb.append(5);
    rb.append(6);
    EXPECT_EQ(rb.size(), 4);
    EXPECT_EQ(rb.front(), 2);
    EXPECT_EQ(rb.back(), 6);

    value = rb.takeBack();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 6);

    rb.removeFront();
    EXPECT_EQ(rb.front(), 4);
}