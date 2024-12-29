#include <gtest/gtest.h>

#include "utils/time/day_schedule.h"

using std::chrono::seconds;
using namespace std::chrono_literals;

class DayScheduleTest : public ::testing::Test {
protected:
    using ScheduleType = DaySchedule<4, int, 5>;
    DaySchedule<4, int, 5> schedule;

    void SetUp() override {
        // Initialize the schedule with some data
        schedule.insertTimePoint(3600s, ScheduleType::ChannelData{1}); // 01:00:00
        schedule.insertTimePoint(7200s, ScheduleType::ChannelData{2}); // 02:00:00
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

TEST_F(DayScheduleTest, FindSlotWithTime) {
    auto slot = schedule.findSlotWithTime(3600s); // 01:00:00
    EXPECT_NE(slot, schedule.end());
    EXPECT_EQ(slot->second, ScheduleType::ChannelData{1});

    slot = schedule.findSlotWithTime(14400s); // Non-existent time
    EXPECT_EQ(slot, schedule.end());
}

TEST_F(DayScheduleTest, GetFirstTimePointOfDay) {
    auto first = schedule.getFirstTimePointOfDay();
    EXPECT_NE(first, schedule.end());
    EXPECT_EQ(first->first, 3600s); // 01:00:00
}

TEST_F(DayScheduleTest, GetLastTimePointOfDay) {
    auto last = schedule.getLastTimePointOfDay();
    EXPECT_NE(last, schedule.end());
    EXPECT_EQ(last->first, 10800s); // 03:00:00
}

TEST_F(DayScheduleTest, GetCurrentTimePointOfDay) {
    auto current = schedule.getCurrentTimePointOfDay(5000s); // 01:23:20
    EXPECT_NE(current, schedule.end());
    EXPECT_EQ(current->first, 3600s); // 01:00:00

    // Returns last possible timepoint
    current = schedule.getCurrentTimePointOfDay(20000s); // Non-existent time
    EXPECT_NE(current, schedule.end());
    EXPECT_EQ(current->first, 10800s); // 03:00:00
}

TEST_F(DayScheduleTest, GetNextTimePointOfDay) {
    auto next = schedule.getNextTimePointOfDay(5000s); // 01:23:20
    EXPECT_NE(next, schedule.end());
    EXPECT_EQ(next->first, 7200s); // 02:00:00

    // There is no next timepoint that day
    next = schedule.getNextTimePointOfDay(20000s); // Non-existent time
    EXPECT_EQ(next, schedule.end());
}
