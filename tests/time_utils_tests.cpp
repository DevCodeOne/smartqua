#include "utils/time/time_utils.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>


namespace {
    class TimeUtilsTests : public ::testing::Test {
    protected:
        std::tm testTime;

        TimeUtilsTests() {
            // Set a known time
            testTime.tm_hour = 10; // 10:00:00
            testTime.tm_min = 0;
            testTime.tm_sec = 0;
        }

        ~TimeUtilsTests() override {
        }

        void SetUp() override {
        }

        void TearDown() override {
        }
    };

    TEST_F(TimeUtilsTests, GetDayOfWeekTest) {
        weekday daysOfWeek[] = {
            weekday::sunday, weekday::monday, weekday::tuesday, weekday::wednesday, weekday::thursday,
            weekday::friday, weekday::saturday
        };

        // Iterate over each day of the week
        for (int i = 0; i < 7; i++) {
            testTime.tm_wday = i; // Set weekday
            weekday expected = daysOfWeek[i];
            weekday result = getDayOfWeek(testTime);
            EXPECT_EQ(expected, result) << "Failed for day " << i;
        }
    }

    TEST_F(TimeUtilsTests, GetTimeOfDayTest) {
        // Expected time elapsed to get to 10 A.M. is 36,000,000 microseconds.
        std::chrono::microseconds expected = std::chrono::hours(10);
        std::chrono::microseconds result = getTimeOfDay<std::chrono::microseconds>(testTime);

        EXPECT_EQ(expected.count(), result.count());
    }

    TEST_F(TimeUtilsTests, SinceWeekBeginningTest) {
        // Test for since the beginning of each day of the week
        for (int i = 0; i < 7; i++) {
            // Monday
            testTime.tm_wday = i; // Set weekday
            std::chrono::microseconds expected = std::chrono::hours(10) + std::chrono::days(i);
            std::chrono::microseconds result = sinceWeekBeginning<std::chrono::microseconds>(testTime);

            EXPECT_EQ(expected.count(), result.count()) << "Failed for day " << i;
        }
    }
}
