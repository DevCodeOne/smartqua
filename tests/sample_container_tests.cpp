#include "utils/container/sample_container.h"

#include <gtest/gtest.h>
#include <chrono>

// Helper function to simulate sampling with time intervals
template <typename T, typename AvgType, uint32_t NSamples>
void simulateSampling(SampleContainer<T, AvgType, NSamples>& container, const std::vector<T>& data, int timeIntervalSeconds) {
    auto currentTime = std::chrono::steady_clock::now();
    for (const auto& value : data) {
        currentTime += std::chrono::seconds(timeIntervalSeconds);
        container.putSample(value, currentTime);
    }
}

// Test 1: Sampling with normal/expected values
TEST(SampleContainerTests, NormalSampling) {
    SampleContainer<float, float, 10> container;
    std::vector<float> data = {10.0f, 10.5f, 11.0f, 11.5f, 12.0f};
    simulateSampling(container, data, 1); // Simulate sampling every second

    ASSERT_EQ(container.size(), data.size());
    EXPECT_NEAR(container.average(), 11.0f, 0.01f);
    EXPECT_FLOAT_EQ(container.last(), data.back());
}

// Test 2: Handling glitches in the data
TEST(SampleContainerTests, GlitchHandling) {
    SampleContainer<float, float, 5> container;

    // Normal values followed by a glitch
    std::vector<float> data = {10.0f, 10.5f, 11.0f, 100.0f}; // 100.0f is a glitch
    auto currentTime = std::chrono::steady_clock::now();

    for (size_t i = 0; i < data.size(); ++i) {
        currentTime += std::chrono::seconds(1);
        bool result = container.putSample(data[i], currentTime);
        if (data[i] == 100.0f) {
            EXPECT_FALSE(result); // Glitch should be rejected
        } else {
            EXPECT_TRUE(result); // Normal values should be accepted
        }
    }

    ASSERT_EQ(container.size(), 3); // Only valid samples
    EXPECT_NEAR(container.average(), (10.0f + 10.5f + 11.0f) / 3, 0.01f);
}

// Test 3: Sampling with a series and ensuring rate of change constraints
TEST(SampleContainerTests, MaxRateOfChangeEnforcement) {
    SampleContainer<float, float, 6> container;

    // Simulate stable sampling
    std::vector<float> stableData = {10.0f, 10.1f, 10.2f, 10.3f};
    simulateSampling(container, stableData, 1);

    // Simulate a sudden spike (glitch)
    auto currentTime = std::chrono::steady_clock::now();
    currentTime += std::chrono::seconds(1);
    bool result = container.putSample(20.0f, currentTime); // Large jump in value

    EXPECT_FALSE(result); // Spike should be rejected
    EXPECT_EQ(container.size(), stableData.size()); // No change in size
}

// Test 4: Metric calculation validation (Average and Variance)
TEST(SampleContainerTests, MetricsCalculation)
{
    SampleContainer<float, float, 6> container;

    // Sample data with a known average and variance
    std::vector<float> data = {10.0f, 12.0f, 14.0f}; // Avg = 12.0, Var = 4.0
    simulateSampling(container, data, 1);

    EXPECT_NEAR(container.average(), 12.0f, 0.01f);
    EXPECT_NEAR(container.variance(), 4.0f, 0.01f); // Variance = Sum((value - mean)^2) / (n - 1)
}

TEST(SampleContainerTests, RepeatedValues) {
    SampleContainer<float, float, 10> container;

    // Repeated same-value samples
    std::vector<float> data = {10.0f, 10.0f, 10.0f};
    simulateSampling(container, data, 1);

    EXPECT_EQ(container.size(), data.size());
    EXPECT_NEAR(container.average(), 10.0f, 0.01f);
    EXPECT_NEAR(container.variance(), 0.0f, 0.01f); // Variance should be zero
}

// Test 6: Initialization of `mMaxRateOfChange`
TEST(SampleContainerTests, MaxRateOfChangeInitialization) {
    SampleContainer<float, float, 10> container;

    // Add some initial stable values
    std::vector<float> stableData = {10.0f, 10.2f, 10.4f, 10.6f};
    simulateSampling(container, stableData, 1);

    // Check if `putSample()` dynamically initializes `mMaxRateOfChange`
    auto currentTime = std::chrono::steady_clock::now();
    currentTime += std::chrono::seconds(1);

    ASSERT_TRUE(container.putSample(10.8f, currentTime)); // Within rate of change
    ASSERT_FALSE(container.putSample(20.0f, currentTime + std::chrono::seconds(1))); // Large jump, should fail
}

// Test 7: Overwriting the oldest samples when capacity exceeds
TEST(SampleContainerTests, RingBufferOverwrite) {
    SampleContainer<float, float, 5> container; // Small buffer size for testing
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}; // Buffer size exceeded
    simulateSampling(container, data, 1);

    // When buffer size is 5, we expect only the last 5 samples to remain
    EXPECT_EQ(container.size(), 5);
    EXPECT_FLOAT_EQ(container.last(), 6.0f); // Last value should be the most recent
}
// TODO: Empty container, how to handle this?
// // Test 8: Empty container behavior
// TEST(SampleContainerTests, EmptyContainer) {
//     SampleContainer<float, float, 10> container;
//
//     // Check averages and size for an empty container
//     EXPECT_EQ(container.size(), 0);
//     EXPECT_FALSE(std::isfinite(container.average()));
// }