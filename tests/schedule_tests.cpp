#include "utils/time/schedule.h"

#include <gtest/gtest.h>

using std::chrono::seconds;

class DayScheduleTest : public ::testing::Test {
protected:
    DaySchedule<int, 5> schedule;

    void SetUp() override {
        // Initialize the schedule with some data
        schedule.insertTimePoint(std::make_pair(seconds(3600), 1)); // 01:00:00
        schedule.insertTimePoint(std::make_pair(seconds(7200), 2)); // 02:00:00
        schedule.insertTimePoint(std::make_pair(seconds(10800), 3)); // 03:00:00
    }
};

TEST_F(DayScheduleTest, InsertTimePoint) {
    EXPECT_TRUE(schedule.insertTimePoint(std::make_pair(seconds(14400), 4))); // 04:00:00
    EXPECT_FALSE(schedule.insertTimePoint(std::make_pair(seconds(3600), 5))); // Duplicate time
}

TEST_F(DayScheduleTest, RemoveTimePoint) {
    EXPECT_TRUE(schedule.removeTimePoint(std::make_pair(seconds(3600), 1))); // 01:00:00
    EXPECT_FALSE(schedule.removeTimePoint(std::make_pair(seconds(14400), 4))); // Non-existent time
}

TEST_F(DayScheduleTest, FindSlotWithTime) {
    auto slot = schedule.findSlotWithTime(seconds(3600)); // 01:00:00
    EXPECT_NE(slot, schedule.end());
    EXPECT_EQ(slot->second, 1);

    slot = schedule.findSlotWithTime(seconds(14400)); // Non-existent time
    EXPECT_EQ(slot, schedule.end());
}

TEST_F(DayScheduleTest, GetFirstTimePointOfDay) {
    auto first = schedule.getFirstTimePointOfDay();
    EXPECT_NE(first, schedule.end());
    EXPECT_EQ(first->first, seconds(3600)); // 01:00:00
}

TEST_F(DayScheduleTest, GetLastTimePointOfDay) {
    auto last = schedule.getLastTimePointOfDay();
    EXPECT_NE(last, schedule.end());
    EXPECT_EQ(last->first, seconds(10800)); // 03:00:00
}

TEST_F(DayScheduleTest, GetCurrentTimePointOfDay) {
    auto current = schedule.getCurrentTimePointOfDay(seconds(5000)); // 01:23:20
    EXPECT_NE(current, schedule.end());
    EXPECT_EQ(current->first, seconds(3600)); // 01:00:00

    // Returns last possible timepoint
    current = schedule.getCurrentTimePointOfDay(seconds(20000)); // Non-existent time
    EXPECT_NE(current, schedule.end());
    EXPECT_EQ(current->first, seconds(10800)); // 03:00:00
}

TEST_F(DayScheduleTest, GetNextTimePointOfDay) {
    auto next = schedule.getNextTimePointOfDay(seconds(5000)); // 01:23:20
    EXPECT_NE(next, schedule.end());
    EXPECT_EQ(next->first, seconds(7200)); // 02:00:00

    // There is no next timepoint that day
    next = schedule.getNextTimePointOfDay(seconds(20000)); // Non-existent time
    EXPECT_EQ(next, schedule.end());
}

class WeekScheduleTest : public ::testing::Test {
protected:
    WeekSchedule<int, 5> weekSchedule;

    void SetUp() override {
        // Initialize the week schedule with some data
        DaySchedule<int, 5> mondaySchedule;
        mondaySchedule.insertTimePoint(std::make_pair(seconds(3600), 1)); // 01:00:00
        mondaySchedule.insertTimePoint(std::make_pair(seconds(7200), 2)); // 02:00:00
        mondaySchedule.insertTimePoint(std::make_pair(seconds(10800), 3)); // 03:00:00

        weekSchedule.setDaySchedule(weekday::monday, mondaySchedule);
    }
};

TEST_F(WeekScheduleTest, SetDaySchedule) {
    DaySchedule<int, 5> tuesdaySchedule;
    tuesdaySchedule.insertTimePoint(std::make_pair(seconds(14400), 4)); // 04:00:00

    weekSchedule.setDaySchedule(weekday::tuesday, tuesdaySchedule);

    auto result = weekSchedule.findCurrentTimePoint(seconds(14400), DaySearchSettings::OnlyThisDay, weekday::tuesday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, 4);
}

TEST_F(WeekScheduleTest, FindCurrentTimePoint) {
    auto result = weekSchedule.findCurrentTimePoint(seconds(5000),
                                                    DaySearchSettings::OnlyThisDay, weekday::monday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, 1);

    result = weekSchedule.findCurrentTimePoint(seconds(20000), DaySearchSettings::OnlyThisDay, weekday::monday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, 3);
}

TEST_F(WeekScheduleTest, FindNextTimePoint) {
    auto result = weekSchedule.findNextTimePoint(seconds(5000),
                                                 DaySearchSettings::OnlyThisDay, weekday::monday);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, 2);

    result = weekSchedule.findNextTimePoint(seconds(20000), DaySearchSettings::OnlyThisDay, weekday::monday);
    EXPECT_FALSE(result.has_value());
}

// TODO: Add tests for ALLDays search settings
