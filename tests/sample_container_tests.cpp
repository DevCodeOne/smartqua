#include "utils/container/sample_container.h"

#include <gtest/gtest.h>

TEST(sample_container_float, basic) {
    sample_container<float, float, 20u> sample;
    for (int i = 0; i < 20; ++i) {
        const int divByTen = i / 10;
        sample.put_sample((static_cast<float>(divByTen) + 1) * 0.5f);
    }

    EXPECT_FLOAT_EQ(sample.average(), 0.75f);
    EXPECT_NEAR(sample.std_variance(), 0.256f, 0.001);
    EXPECT_NEAR(sample.variance(), 0.066f, 0.001);
    EXPECT_FLOAT_EQ(sample.last(), 1.0f);

    for (int i = 0; i < 20; ++i) {
        const int divByTen = i / 10;
        sample.put_sample((static_cast<float>(divByTen) + 1) * 0.5f);
    }

    EXPECT_FLOAT_EQ(sample.average(), 0.75f);
    EXPECT_NEAR(sample.std_variance(), 0.256f, 0.001);
    EXPECT_NEAR(sample.variance(), 0.066f, 0.001);
    EXPECT_FLOAT_EQ(sample.last(), 1.0f);

    for (int i = 0; i < 10; ++i) {
        const int divByTen = i / 10;
        sample.put_sample((static_cast<float>(divByTen) + 1) * 0.5f);
    }

    // Overwrite oldest values which were 0.5f before, so no changes
    EXPECT_FLOAT_EQ(sample.average(), 0.75f);
    EXPECT_NEAR(sample.std_variance(), 0.256f, 0.001);
    EXPECT_NEAR(sample.variance(), 0.066f, 0.001);
    EXPECT_FLOAT_EQ(sample.last(), 0.5f);


    // Overwrite last values which are 1.0f
    for (int i = 0; i < 10; ++i) {
        const int divByTen = i / 10;
        sample.put_sample((static_cast<float>(divByTen) + 1) * 0.5f);
    }

    EXPECT_FLOAT_EQ(sample.average(), 0.5f);
    EXPECT_NEAR(sample.variance(), 0, 0.001f);
    EXPECT_NEAR(sample.std_variance(), 0, 0.001f);
    EXPECT_FLOAT_EQ(sample.last(), 0.5f);

    sample_container<float, float, 20> sample2 = sample;

    EXPECT_EQ(sample2.size(), sample.size());
    EXPECT_EQ(sample2.last(), sample.last());
    EXPECT_EQ(sample2.average(), sample.average());
}