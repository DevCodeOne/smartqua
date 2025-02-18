#include "utils/time/schedule_tracker.h"
#include "utils/time/schedule.h"

#include <gtest/gtest.h>

class ScheduleTrackerTests : public ::testing::Test {
    protected:
    using TestWeekScheduleType = WeekSchedule<2, float, 10>;
    using TestDayScheduleType = TestWeekScheduleType::DayScheduleType;
    using ChannelData = TestDayScheduleType::ChannelData;

    TestWeekScheduleType schedule;
    ScheduleTracker<TestWeekScheduleType, float, 4> tracker;

    ScheduleTrackerTests() : tracker(&schedule, ScheduleEventTransitionMode::SingleShot) {
        using namespace std::chrono_literals;
        TestDayScheduleType monday;
        monday.insertTimePoint(10h, ChannelData{1.0f});
        monday.insertTimePoint(10h + 30min, ChannelData{2.0f});
        monday.insertTimePoint(11h + 30min, ChannelData{3.0f});

        TestDayScheduleType tuesday;
        tuesday.insertTimePoint(9h, ChannelData{10.0f});
        tuesday.insertTimePoint(11h + 30min, ChannelData{20.0f});
        tuesday.insertTimePoint(12h + 30min, ChannelData{30.0f});

        TestDayScheduleType repeating;
        tuesday.insertTimePoint(12h, ChannelData{11.0f});
        tuesday.insertTimePoint(13h + 30min, ChannelData{24.0f});
        tuesday.insertTimePoint(15h + 30min, ChannelData{36.0f});

        schedule.setDaySchedule(WeekDay::monday, monday);
        schedule.setDaySchedule(WeekDay::tuesday, tuesday);
        schedule.setDaySchedule(WeekDay::wednesday, repeating);
        schedule.setDaySchedule(WeekDay::thursday, repeating);
        schedule.setDaySchedule(WeekDay::friday, repeating);
        schedule.setDaySchedule(WeekDay::saturday, repeating);
        schedule.setDaySchedule(WeekDay::sunday, repeating);
    }
};

// Helper function to create a std::tm object
std::tm makeTime(int weekday, int hour, int minute, int second = 0) {
    std::tm date{};
    date.tm_wday = weekday;  // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    date.tm_hour = hour;
    date.tm_min = minute;
    date.tm_sec = second;
    return date;
}

// Test for SingleShot Behavior
TEST_F(ScheduleTrackerTests, SingleShotBehavior) {
    tracker.setTrackingType(ScheduleEventTransitionMode::SingleShot);

    // Set currentDate to Monday, 10:05:00
    std::tm currentDate = makeTime(1, 10, 5);

    // Retrieve the first event value
    auto value = tracker.getCurrentChannelValue(0, currentDate);

    // Expect the first event (10:00) to be returned
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(value.value(), 1.0f);

    // Other channels don't have a value
    EXPECT_FALSE(tracker.getCurrentChannelValue(1, currentDate).has_value());
    EXPECT_FALSE(tracker.getCurrentChannelValue(2, currentDate).has_value());
    EXPECT_FALSE(tracker.getCurrentChannelValue(3, currentDate).has_value());

    // Other channels don't have a value -> multi value check
    auto channelValues = tracker.getCurrentChannelValues(currentDate);
    EXPECT_FALSE(channelValues[1].has_value());
    EXPECT_FALSE(channelValues[2].has_value());
    EXPECT_FALSE(channelValues[3].has_value());

    // Update channel time to signal successful update
    tracker.updateAllChannelTimes(currentDate);

    // Call it again to ensure it only returns once (per SingleShot behavior)
    value = tracker.getCurrentChannelValue(0, currentDate);
    EXPECT_FALSE(value.has_value());

    // TODO: check for edge cases
}

// Test for Hold Behavior
TEST_F(ScheduleTrackerTests, HoldBehavior) {
    tracker.setTrackingType(ScheduleEventTransitionMode::Hold);

    // Set currentDate to Monday, 10:35:00
    std::tm currentDate = makeTime(1, 10, 35);

    // Retrieve the current event value
    auto value = tracker.getCurrentChannelValue(0, currentDate);

    // Expect the second event (10:30) since 10:35 > 10:30
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(value.value(), 2.0f);

    // Other channels don't have a value
    EXPECT_FALSE(tracker.getCurrentChannelValue(1, currentDate).has_value());
    EXPECT_FALSE(tracker.getCurrentChannelValue(2, currentDate).has_value());
    EXPECT_FALSE(tracker.getCurrentChannelValue(3, currentDate).has_value());

    // Other channels don't have a value -> multi value check
    auto channelValues = tracker.getCurrentChannelValues(currentDate);
    EXPECT_FALSE(channelValues[1].has_value());
    EXPECT_FALSE(channelValues[2].has_value());
    EXPECT_FALSE(channelValues[3].has_value());

    // Update channel time to signal successful update
    tracker.updateAllChannelTimes(currentDate);

    // Call it again to ensure Hold behavior keeps returning the value
    value = tracker.getCurrentChannelValue(0, currentDate);
    ASSERT_TRUE(value.has_value());
    EXPECT_FLOAT_EQ(value.value(), 2.0f);
}

// Test for Interpolation Behavior
TEST_F(ScheduleTrackerTests, InterpolationBehavior) {
    tracker.setTrackingType(ScheduleEventTransitionMode::Interpolation);

    // Set currentDate to Monday, 10:45:00 (between 10:30 and 11:30)
    std::tm currentDate = makeTime(1, 10, 45);

    // Retrieve the interpolated value
    auto value = tracker.getCurrentChannelValue(0, currentDate);

    // Expect interpolation between 2.0 (10:30) and 3.0 (11:30)
    // Interpolation factor = (15 minutes after 10:30) / 60 minutes = 0.25
    ASSERT_TRUE(value.has_value());
    float interpolatedValue = 2.0f + 0.25f * (3.0f - 2.0f);
    EXPECT_FLOAT_EQ(value.value(), interpolatedValue);

    // Other channels don't have a value
    EXPECT_FALSE(tracker.getCurrentChannelValue(1, currentDate).has_value());
    EXPECT_FALSE(tracker.getCurrentChannelValue(2, currentDate).has_value());
    EXPECT_FALSE(tracker.getCurrentChannelValue(3, currentDate).has_value());

    // Other channels don't have a value -> multi value check
    auto channelValues = tracker.getCurrentChannelValues(currentDate);
    EXPECT_FALSE(channelValues[1].has_value());
    EXPECT_FALSE(channelValues[2].has_value());
    EXPECT_FALSE(channelValues[3].has_value());

    // Update channel time to signal successful update
    tracker.updateAllChannelTimes(currentDate);
}

// Test for Invalid Channel Index
TEST_F(ScheduleTrackerTests, InvalidChannelIndex) {
    std::tm currentDate = makeTime(1, 10, 0);

    // Retrieve a value using an invalid channel index
    auto value = tracker.getCurrentChannelValue(5, currentDate);  // Index 5 exceeds NumChannels

    // Expect no value to be returned
    EXPECT_FALSE(value.has_value());
}

// Test for No Event Found
TEST_F(ScheduleTrackerTests, NoEventFound) {
    tracker.setTrackingType(ScheduleEventTransitionMode::SingleShot);

    // Set currentDate to Monday, 09:00:00 (before the first event)
    std::tm currentDate = makeTime(1, 9, 0);

    // Retrieve the current event value
    auto value = tracker.getCurrentChannelValue(0, currentDate);

    // Expect no value to be returned since no event has started yet
    EXPECT_FALSE(value.has_value());
}

TEST_F(ScheduleTrackerTests, EventAfterWrapAround) {
    struct Event { int eventData; std::chrono::seconds eventTime; };
    TrackingData<Event, std::chrono::seconds> trackingData {
        .current = Event{1, std::chrono::hours{1}},
        .next = std::nullopt,
        .channelTime = std::chrono::days{6} + std::chrono::hours{23},
        .currentEventInEffectSince = std::chrono::days{6} + std::chrono::hours{23} - std::chrono::hours{1},
        .now = std::chrono::hours{2}
    };

    SingleShotTracker<int> tracker;
    auto value = tracker.getChannelValue(trackingData);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 1);
}
