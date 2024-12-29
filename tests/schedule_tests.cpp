#include "utils/time/schedule.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

class WeekScheduleTest : public ::testing::Test {
protected:
    using ScheduleType = WeekSchedule<4, int, 5>;
    using DayScheduleType = ScheduleType::DayScheduleType;
    using ChannelData = DayScheduleType::ChannelData;

    ScheduleType weekSchedule;

    void SetUp() override {
        // Initialize the week schedule with some data
        DayScheduleType mondaySchedule;
        mondaySchedule.insertTimePoint(3600s, ChannelData{1}); // 01:00:00
        mondaySchedule.insertTimePoint(7200s, ChannelData{2}); // 02:00:00
        mondaySchedule.insertTimePoint(10800s, ChannelData{3}); // 03:00:00

        weekSchedule.setDaySchedule(weekday::monday, mondaySchedule);
    }
};

TEST_F(WeekScheduleTest, SetDaySchedule) {
    DayScheduleType tuesdaySchedule;
    tuesdaySchedule.insertTimePoint(14400s, ChannelData{4}); // 04:00:00

    weekSchedule.setDaySchedule(weekday::tuesday, tuesdaySchedule);

    auto result = weekSchedule.findCurrentTimePoint(14400s, DaySearchSettings::OnlyThisDay, weekday::tuesday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, ChannelData{4});
}

TEST_F(WeekScheduleTest, FindCurrentTimePoint) {
    auto result = weekSchedule.findCurrentTimePoint(5000s,
                                                    DaySearchSettings::OnlyThisDay, weekday::monday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, ChannelData{1});

    result = weekSchedule.findCurrentTimePoint(20000s, DaySearchSettings::OnlyThisDay, weekday::monday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, ChannelData{3});
}

TEST_F(WeekScheduleTest, FindNextTimePoint) {
    auto result = weekSchedule.findNextTimePoint(5000s,
                                                 DaySearchSettings::OnlyThisDay, weekday::monday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, ChannelData{2});

    result = weekSchedule.findNextTimePoint(20000s, DaySearchSettings::OnlyThisDay, weekday::monday);
    EXPECT_FALSE(result.has_value());
}

// TODO: Add tests for ALLDays search settings
