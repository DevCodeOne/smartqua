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

        weekSchedule.setDaySchedule(WeekDay::monday, mondaySchedule);
    }
};

TEST_F(WeekScheduleTest, SetDaySchedule) {
    DayScheduleType tuesdaySchedule;
    tuesdaySchedule.insertTimePoint(14400s, ChannelData{4}); // 04:00:00

    weekSchedule.setDaySchedule(WeekDay::tuesday, tuesdaySchedule);

    auto result = weekSchedule.findCurrentEventStatus(14400s, WeekDay::tuesday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 4);
}

TEST_F(WeekScheduleTest, FindCurrentTimePoint) {
    auto result = weekSchedule.findCurrentEventStatus(5000s,
                                                     WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 1);

    result = weekSchedule.findCurrentEventStatus(20000s, WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 3);
}

TEST_F(WeekScheduleTest, FindNextTimePoint) {
    auto result = weekSchedule.findNextEventStatus(5000s,
                                                   WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 2);

    result = weekSchedule.findNextEventStatus(20000s, WeekDay::monday, DaySearchSettings::OnlyThisDay);
    EXPECT_FALSE(std::ranges::all_of(result, [](const auto &channelValue) { return channelValue.has_value(); }));
}

// TODO: Add tests for AllDays search settings
// TODO: Also tests multiple channels
