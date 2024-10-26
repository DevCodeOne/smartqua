#include "utils/container/ring_buffer.h"
#include "utils/container/sample_container.h"
#include "utils/container/lookup_table.h"

#include <gtest/gtest.h>

TEST(ring_buffer, basic) {
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
