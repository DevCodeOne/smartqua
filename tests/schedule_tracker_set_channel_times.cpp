#include "utils/time/schedule_tracker.h"
#include "utils/time/schedule.h"

#include <gtest/gtest.h>

class ScheduleTrackerSetChannelTimesTests : public ::testing::Test {
protected:
    using TestWeekScheduleType = WeekSchedule<4, float, 10>;
    using TestDayScheduleType = TestWeekScheduleType::DayScheduleType;
    using MinimalTimeUnit = std::chrono::seconds;
    using ChannelTimesArray = std::array<MinimalTimeUnit, 4>;

    TestWeekScheduleType schedule;
    ScheduleTracker<TestWeekScheduleType, float, 4> tracker;

    ScheduleTrackerSetChannelTimesTests() : tracker(&schedule, ScheduleEventTransitionMode::SingleShot) {
        using namespace std::chrono_literals;

        // Example: adding events for Monday
        TestDayScheduleType monday;
        monday.insertTimePoint(8h, {1.0f, 2.0f});
        monday.insertTimePoint(12h, {3.0f, 4.0f});

        TestDayScheduleType tuesday;
        tuesday.insertTimePoint(10h, {5.0f, 6.0f});

        schedule.setDaySchedule(WeekDay::monday, monday);
        schedule.setDaySchedule(WeekDay::tuesday, tuesday);
        // Note: other days are left empty (no events).
    }
};

// Test when all channel times are valid and correspond to events
TEST_F(ScheduleTrackerSetChannelTimesTests, AllChannelTimesValid) {
    using namespace std::chrono_literals;
    ChannelTimesArray channelTimes{
        1 * 24h + 8h, // Monday 08:00
        2 * 24h + 10h, // Tuesday 10:00
        1 * 24h + 12h, // Monday 12:00
        1 * 24h + 12h // Monday 12:00
    };

    tracker.setChannelTimes(channelTimes);

    // Ensure values are preserved
    const auto& result = tracker.getChannelTimes();
    EXPECT_EQ(result[0], 1 * 24h + 8h);  // Monday 08:00
    EXPECT_EQ(result[1], 2 * 24h + 10h); // Tuesday 10:00
    EXPECT_NE(result[2], 1 * 24h + 12h); // No channel data, no time
    EXPECT_NE(result[3], 1 * 24h + 12h); // No channel data, no time
}

// Test behavior when a channel time is out of range (negative day)
TEST_F(ScheduleTrackerSetChannelTimesTests, ChannelTimeOutOfRangeLow) {
    using namespace std::chrono_literals;
    ChannelTimesArray channelTimes{
        -24h, // Negative day
        1 * 24h + 8h, // Monday 08:00
        2 * 24h + 10h, // Tuesday 10:00
        1 * 24h + 12h  // Monday 12:00
    };

    tracker.setChannelTimes(channelTimes);

    // Ensure valid times are preserved and invalid ones are not updated
    const auto& result = tracker.getChannelTimes();
    EXPECT_EQ(result[0], 0s);            // Invalid time clamped to default (or undefined behavior)
    EXPECT_EQ(result[1], 1 * 24h + 8h);  // Monday 08:00
    EXPECT_NE(result[2], 2 * 24h + 10h); // No event, No channelTime
    EXPECT_NE(result[3], 1 * 24h + 12h); // No event, No channelTime
}

// Test behavior when a channel time exceeds the maximum valid week range
TEST_F(ScheduleTrackerSetChannelTimesTests, ChannelTimeOutOfRangeHigh) {
    using namespace std::chrono_literals;
    ChannelTimesArray channelTimes{
        7 * 24h + 1h, // Exceeds Sunday by 1 hour
        1 * 24h + 8h, // Monday 08:00
        2 * 24h + 10h, // Tuesday 10:00
        1 * 24h + 12h  // Monday 12:00
    };

    tracker.setChannelTimes(channelTimes);

    // Ensure valid times are preserved and invalid ones are clamped
    const auto& result = tracker.getChannelTimes();
    EXPECT_EQ(result[0], 0s);            // Invalid time clamped to default (or undefined behavior)
    EXPECT_EQ(result[1], 1 * 24h + 8h);  // Monday 08:00
    EXPECT_NE(result[2], 2 * 24h + 10h); // Tuesday 10:00
    EXPECT_NE(result[3], 1 * 24h + 12h); // Monday 12:00
}

// Test when a time corresponds to a day without events
TEST_F(ScheduleTrackerSetChannelTimesTests, ChannelTimeNoEvents) {
    using namespace std::chrono_literals;
    ChannelTimesArray channelTimes{
        6 * 24h + 8h, // Saturday 08:00 (no events defined)
        1 * 24h + 8h, // Monday 08:00
        2 * 24h + 10h, // Tuesday 10:00
        1 * 24h + 12h  // Monday 12:00
    };

    tracker.setChannelTimes(channelTimes);

    // Ensure valid times are preserved
    const auto& result = tracker.getChannelTimes();
    EXPECT_EQ(result[0], 2 * 24h + 10h); // Clamped to last valid event tuesday 10:00
    EXPECT_EQ(result[1], 1 * 24h + 8h);  // Monday 08:00
    EXPECT_NE(result[2], 2 * 24h + 10h); // No channel event, no channel time
    EXPECT_NE(result[3], 1 * 24h + 12h); // No channel event, no channel time
}

// Test that out-of-range times are clamped to the last known valid event
TEST_F(ScheduleTrackerSetChannelTimesTests, ClampingToLastValidEvent)
{
    using namespace std::chrono_literals;
    ChannelTimesArray channelTimes{
        1 * 24h + 9h, // Monday 09:00 (between events at 08:00 and 12:00)
        4 * 24h + 15h, // Thursday 15:00 (no events)
        -1 * 24h + 1h, // Negative time
        7 * 24h + 1h   // Exceeds Sunday
    };

    tracker.setChannelTimes(channelTimes);

    // Ensure valid times are clamped appropriately
    const auto& result = tracker.getChannelTimes();
    EXPECT_EQ(result[0], 1 * 24h + 8h);  // Clamped to Monday 08:00
    EXPECT_EQ(result[1], 2 * 24h + 10h); // Clamped back to Tuesday 10:00
    EXPECT_EQ(result[2], 0s);            // Invalid time clamped to default
    EXPECT_EQ(result[3], 0s);            // Invalid time clamped to default
}
