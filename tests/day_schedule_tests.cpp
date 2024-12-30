#include <gtest/gtest.h>

#include "utils/time/day_schedule.h"

using std::chrono::seconds;
using namespace std::chrono_literals;

class DayScheduleTest : public ::testing::Test {
protected:
    using ScheduleType = DaySchedule<4, int, 10>;
    ScheduleType schedule;

    void SetUp() override {
        // Initialize the schedule with some data
        schedule.insertTimePoint(3600s, ScheduleType::ChannelData{1}); // 01:00:00
        schedule.insertTimePoint(3600s + 30min, ScheduleType::ChannelData{{0, 1}}); // 01:30:00
        schedule.insertTimePoint(7200s, ScheduleType::ChannelData{2}); // 02:00:00
        schedule.insertTimePoint(7200s + 30min, ScheduleType::ChannelData{{4, 2}}); // 02:00:00
        schedule.insertTimePoint(3600s, ScheduleType::ChannelData{{{}, 1}}); // 01:00:00
        schedule.insertTimePoint(10800s, ScheduleType::ChannelData{3}); // 03:00:00
    }
};

TEST_F(DayScheduleTest, InsertTimePoint) {
    EXPECT_TRUE(schedule.insertTimePoint(14400s, ScheduleType::ChannelData{4})); // 04:00:00
    EXPECT_FALSE(schedule.insertTimePoint(3600s, ScheduleType::ChannelData{5})); // Duplicate time
}

TEST_F(DayScheduleTest, RemoveTimePoint) {
    EXPECT_TRUE(schedule.removeTimePoint(3600s)); // 01:00:00
    EXPECT_FALSE(schedule.removeTimePoint(14400s)); // Non-existent time
}

TEST_F(DayScheduleTest, GetFirstTimePointOfDay) {
    auto first = schedule.getFirstTimePointOfDay(0);
    EXPECT_NE(first, std::nullopt);
    EXPECT_EQ(first->first, 3600s); // 01:00:00
    EXPECT_EQ(first->second, 1);
}

TEST_F(DayScheduleTest, GetFirstTimePointOfDayChannel1) {
    auto first = schedule.getFirstTimePointOfDay(1);
    EXPECT_NE(first, std::nullopt);
    EXPECT_EQ(first->first, 3600s + 30min); // 01:30:00
    EXPECT_EQ(first->second, 1);
}

TEST_F(DayScheduleTest, GetLastTimePointOfDay) {
    auto last = schedule.getLastTimePointOfDay(0);
    EXPECT_NE(last, std::nullopt);
    EXPECT_EQ(last->first, 10800s); // 03:00:00
    EXPECT_EQ(last->second, 3);
}

TEST_F(DayScheduleTest, GetLastTimePointOfDayChannel1) {
    auto last = schedule.getLastTimePointOfDay(1);
    EXPECT_NE(last, std::nullopt);
    EXPECT_EQ(last->first, 7200s + 30min); // 02:30:00
    EXPECT_EQ(last->second, 2);
}

TEST_F(DayScheduleTest, GetCurrentTimePointOfDay) {
    auto current = schedule.getCurrentTimePointOfDay(0, 5000s); // 01:23:20
    EXPECT_NE(current, std::nullopt);
    EXPECT_EQ(current->first, 3600s); // 01:00:00
    EXPECT_EQ(current->second, 1);

    // Returns last possible timepoint
    current = schedule.getCurrentTimePointOfDay(0, 20000s); // Non-existent time
    EXPECT_NE(current, std::nullopt);
    EXPECT_EQ(current->first, 10800s); // 03:00:00
    EXPECT_EQ(current->second, 3);
}

TEST_F(DayScheduleTest, GetCurrentTimePointOfDayChannel1) {
    auto current = schedule.getCurrentTimePointOfDay(1, 3600s + 50min); // 01:50:20
    EXPECT_NE(current, std::nullopt);
    EXPECT_EQ(current->first, 3600s + 30min); // 01:30:00
    EXPECT_EQ(current->second, 1);

    // Returns last possible timepoint
    current = schedule.getCurrentTimePointOfDay(1, 20000s); // Non-existent time
    EXPECT_NE(current, std::nullopt);
    EXPECT_EQ(current->first, 7200s + 30min); // 03:00:00
    EXPECT_EQ(current->second, 2);
}

TEST_F(DayScheduleTest, GetNextTimePointOfDay) {
    auto next = schedule.getNextTimePointOfDay(0, 5000s); // 01:23:20
    EXPECT_NE(next, std::nullopt);
    EXPECT_EQ(next->first, 3600s + 30min); // 01:30:00
    EXPECT_EQ(next->second, 0);

    // There is no next timepoint that day
    next = schedule.getNextTimePointOfDay(0, 20000s); // Non-existent time
    EXPECT_EQ(next, std::nullopt);
}
