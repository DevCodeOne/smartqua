#include "utils/time/time_utils.h"

#include <gtest/gtest.h>

class TimeUtilsTests : public ::testing::Test {
protected:
    std::tm testTime;

    TimeUtilsTests() :testTime{} {
        // Set a known time
        testTime.tm_hour = 10; // 10:00:00
        testTime.tm_min = 0;
        testTime.tm_sec = 0;
    }

    ~TimeUtilsTests() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(TimeUtilsTests, GetDayOfWeekTest) {
    WeekDay daysOfWeek[] = {
        WeekDay::sunday, WeekDay::monday, WeekDay::tuesday, WeekDay::wednesday, WeekDay::thursday,
        WeekDay::friday, WeekDay::saturday
    };

    // Iterate over each day of the week
    for (int i = 0; i < 7; i++) {
        testTime.tm_wday = i; // Set weekday
        WeekDay expected = daysOfWeek[i];
        WeekDay result = getDayOfWeek(testTime);
        EXPECT_EQ(expected, result) << "Failed for day " << i;
    }
}

TEST_F(TimeUtilsTests, GetTimeOfDayTest) {
    // Expected time elapsed to get to 10 A.M. is 36,000,000 seconds.
    std::chrono::seconds expected = std::chrono::hours(10);
    std::chrono::seconds result = getTimeOfDay<std::chrono::seconds>(testTime);

    EXPECT_EQ(expected.count(), result.count());
}

TEST_F(TimeUtilsTests, SinceWeekBeginningTest) {
    // Test for since the beginning of each day of the week
    for (int i = 0; i < 7; i++) {
        // Monday
        testTime.tm_wday = i; // Set weekday
        std::chrono::seconds expected = std::chrono::hours(10) + std::chrono::days(i);
        std::chrono::seconds result = sinceWeekBeginning<std::chrono::seconds>(testTime);

        EXPECT_EQ(expected.count(), result.count()) << "Failed for day " << i;
    }
}

TEST_F(TimeUtilsTests, IsLargerOrEqualDurationForSeconds) {
    // Test types of timeunits
    using CurrentType = std::chrono::seconds;

    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::seconds>::value));
    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::minutes>::value));
    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::hours>::value));
    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::days>::value));
    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::weeks>::value));
    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::years>::value));

    EXPECT_FALSE((IsLargerOrEqualDuration<CurrentType, std::chrono::milliseconds>::value));
    EXPECT_FALSE((IsLargerOrEqualDuration<CurrentType, std::chrono::microseconds>::value));
}

TEST_F(TimeUtilsTests, IsLargerOrEqualDurationForDays) {
    // Test types of timeunits
    using CurrentType = std::chrono::days;

    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::days>::value));
    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::weeks>::value));
    EXPECT_TRUE((IsLargerOrEqualDuration<CurrentType, std::chrono::years>::value));

    EXPECT_FALSE((IsLargerOrEqualDuration<CurrentType, std::chrono::seconds>::value));
    EXPECT_FALSE((IsLargerOrEqualDuration<CurrentType, std::chrono::minutes>::value));
    EXPECT_FALSE((IsLargerOrEqualDuration<CurrentType, std::chrono::hours>::value));
    EXPECT_FALSE((IsLargerOrEqualDuration<CurrentType, std::chrono::milliseconds>::value));
    EXPECT_FALSE((IsLargerOrEqualDuration<CurrentType, std::chrono::microseconds>::value));
}

TEST_F(TimeUtilsTests, diffWithDurationSinceWeekBeginning) {
    auto makeTime = [](const auto days, const auto hours, const auto minutes) {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::hours(hours) + std::chrono::minutes(minutes) + std::chrono::days(days));
    };

    auto beginning = makeTime(0, 0, 0);
    auto beginningLastWeek = makeTime(6, 23, 0);
    auto end = makeTime(0, 1, 0);

    EXPECT_EQ(diffWithDurationSinceWeekBeginning(end, beginning), std::chrono::hours(1));
    EXPECT_EQ(diffWithDurationSinceWeekBeginning(end, beginning), std::chrono::hours(1));
    EXPECT_EQ(diffWithDurationSinceWeekBeginning(end, beginningLastWeek), std::chrono::hours(2));
}
