#include "utils/container/sample_container.h"

#include <gtest/gtest.h>

TEST(sample_container_float, basic) {
    SampleContainer<float, float, 20u> sample(1000.0f);
    auto currentTime = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) {
        const int divByTen = i / 10;
        currentTime += std::chrono::seconds(1);
        sample.putSample((static_cast<float>(divByTen) + 1) * 0.5f, currentTime);
    }

    EXPECT_FLOAT_EQ(sample.average(), 0.75f);
    EXPECT_NEAR(sample.stdVariance(), 0.256f, 0.01);
    EXPECT_NEAR(sample.variance(), 0.066f, 0.001);
    EXPECT_FLOAT_EQ(sample.last(), 1.0f);

    for (int i = 0; i < 20; ++i) {
        const int divByTen = i / 10;
        currentTime += std::chrono::seconds(1);
        sample.putSample((static_cast<float>(divByTen) + 1) * 0.5f, currentTime);
    }

    EXPECT_FLOAT_EQ(sample.average(), 0.75f);
    EXPECT_NEAR(sample.stdVariance(), 0.256f, 0.001);
    EXPECT_NEAR(sample.variance(), 0.066f, 0.001);
    EXPECT_FLOAT_EQ(sample.last(), 1.0f);

    for (int i = 0; i < 10; ++i) {
        const int divByTen = i / 10;
        currentTime += std::chrono::seconds(1);
        sample.putSample((static_cast<float>(divByTen) + 1) * 0.5f, currentTime);
    }

    // Overwrite oldest values which were 0.5f before, so no changes
    EXPECT_FLOAT_EQ(sample.average(), 0.75f);
    EXPECT_NEAR(sample.stdVariance(), 0.256f, 0.001);
    EXPECT_NEAR(sample.variance(), 0.066f, 0.001);
    EXPECT_FLOAT_EQ(sample.last(), 0.5f);

    // Overwrite last values which are 1.0f
    for (int i = 0; i < 10; ++i) {
        const int divByTen = i / 10;
        currentTime += std::chrono::seconds(1);
        sample.putSample((static_cast<float>(divByTen) + 1) * 0.5f, currentTime);
    }

    EXPECT_FLOAT_EQ(sample.average(), 0.5f);
    EXPECT_NEAR(sample.variance(), 0, 0.001f);
    EXPECT_NEAR(sample.stdVariance(), 0, 0.001f);
    EXPECT_FLOAT_EQ(sample.last(), 0.5f);

    SampleContainer<float, float, 20> sample2 = sample;

    EXPECT_EQ(sample2.size(), sample.size());
    EXPECT_EQ(sample2.last(), sample.last());
    EXPECT_EQ(sample2.average(), sample.average());

    // Testing new putSample features
    SampleContainer<float, float, 20> sample3(1000);

    for (int i = 0; i < 4; ++i) {
        currentTime += std::chrono::seconds(1);
        sample3.putSample(2.0f, currentTime);
    }
    // Add 2.0f four times
    EXPECT_FLOAT_EQ(sample3.average(), 2.0f);
    EXPECT_FLOAT_EQ(sample3.variance(), 0.0f);

    for (int i = 0; i < 4; ++i) {
        currentTime += std::chrono::seconds(1);
        sample3.putSample(2.0f, currentTime);
    }
    // Add 2.0f four times
    EXPECT_FLOAT_EQ(sample3.average(), 2.0f);
    EXPECT_FLOAT_EQ(sample3.variance(), 0.0f);
    EXPECT_FLOAT_EQ(sample3.stdVariance(), 0.0f);

    for (int i = 0; i < 6; ++i) {
        currentTime += std::chrono::seconds(1);
        sample3.putSample(1.0f, currentTime);
    }
    // Add 1.0f six times
    EXPECT_NEAR(sample3.average(), 1.57f, 0.01f);
    EXPECT_FLOAT_EQ(sample3.last(), 1.0f);

    for (int i = 0; i < 10; ++i) {
        currentTime += std::chrono::seconds(1);
        sample3.putSample(3.0f, currentTime);
    }
    // Add 3.0f ten times (filling the container)
    EXPECT_FLOAT_EQ(sample3.average(), 2.2f);
    EXPECT_FLOAT_EQ(sample3.last(), 3.0f);
    EXPECT_NEAR(sample3.stdVariance(), 0.89f, 0.01f);
}
