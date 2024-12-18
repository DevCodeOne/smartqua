#include "utils/container/lookup_table.h"

#include <gtest/gtest.h>

TEST(ResourceLookupTable, basic) {
    ResourceLookupTable<ThreadSafety::Safe, 0, 1, 2, 4> resources;

    // Doesn't exist should never return a valid flag
    EXPECT_FALSE(resources.setIf(5, true));
    EXPECT_FALSE(resources.setIfValue(5, true, false));
    EXPECT_FALSE(resources.setIfValue(5, true, true));

    // Should return valid flag and immediately be returned, since it isn't stored
    EXPECT_TRUE(resources.setIfValue(4, true, false));
    EXPECT_TRUE(resources.setIfValue(4, true, false));

    // Should return valid flag, which is stored, so the second call should fail
    {
        auto flag = resources.setIfValue(4, true, false);
        EXPECT_FALSE(resources.setIfValue(4, true, false));
    }

    // Flag is destroyed, so resource is free again
    EXPECT_TRUE(resources.setIfValue(4, true, false));
}