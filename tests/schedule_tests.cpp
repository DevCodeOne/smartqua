#include "utils/time/schedule.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

class WeekScheduleSingleDayTest : public ::testing::Test {
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

TEST_F(WeekScheduleSingleDayTest, SetDaySchedule) {
    DayScheduleType tuesdaySchedule;
    tuesdaySchedule.insertTimePoint(14400s, ChannelData{4}); // 04:00:00

    weekSchedule.setDaySchedule(WeekDay::tuesday, tuesdaySchedule);

    auto result = weekSchedule.findCurrentEventStatus(14400s, WeekDay::tuesday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 4);
}

TEST_F(WeekScheduleSingleDayTest, FindCurrentTimePoint) {
    auto result = weekSchedule.findCurrentEventStatus(5000s,
                                                     WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 1);

    result = weekSchedule.findCurrentEventStatus(20000s, WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 3);
}

TEST_F(WeekScheduleSingleDayTest, FindNextTimePoint) {
    auto result = weekSchedule.findNextEventStatus(5000s,
                                                   WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 2);

    result = weekSchedule.findNextEventStatus(20000s, WeekDay::monday, DaySearchSettings::OnlyThisDay);
    EXPECT_FALSE(std::ranges::all_of(result, [](const auto &channelValue) { return channelValue.has_value(); }));
}

// TODO: Also tests multiple channels
class WeekScheduleRepeatingDayTest : public ::testing::Test {
protected:
    using ScheduleType = WeekSchedule<4, int, 5>;
    using DayScheduleType = ScheduleType::DayScheduleType;
    using ChannelData = DayScheduleType::ChannelData;

    static constexpr std::array<std::chrono::seconds, 3> timePoints{3600s, 7200s, 10800s};
    static constexpr std::array<int, 3> channelData{1, 2, 3};

    ScheduleType weekSchedule;

    void SetUp() override {
        // Initialize the week schedule with some data
        DayScheduleType mondaySchedule;
        for (int i = 0; i < 3; ++i)
        {
            mondaySchedule.insertTimePoint(timePoints[i], ChannelData{channelData[i]});
        }

        for (int i = 0; i < 7; ++i)
        {
            weekSchedule.setDaySchedule(static_cast<WeekDay>(i), mondaySchedule);
        }
    }
};

TEST_F(WeekScheduleRepeatingDayTest, FindCurrentTimePoint) {
    auto result = weekSchedule.findCurrentEventStatus(5000s,
                                                     WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 1);

    result = weekSchedule.findCurrentEventStatus(20000s, WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 3);
}

TEST_F(WeekScheduleRepeatingDayTest, FindNextTimePoint) {
    auto result = weekSchedule.findNextEventStatus(5000s,
                                                   WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 2);

    result = weekSchedule.findNextEventStatus(20000s, WeekDay::monday, DaySearchSettings::OnlyThisDay);
    EXPECT_FALSE(std::ranges::all_of(result, [](const auto &channelValue) { return channelValue.has_value(); }));
}

TEST_F(WeekScheduleRepeatingDayTest, FindCurrentTimePointLastPointOfToday) {
    auto result = weekSchedule.findCurrentEventStatus(timePoints[2] + 20s,
                                                     WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, channelData[2]);
}

TEST_F(WeekScheduleRepeatingDayTest, FindNextTimePointLastPointOfTodayOnlyToday) {
    auto result = weekSchedule.findNextEventStatus(timePoints[2] + 20s,
                                                   WeekDay::monday, DaySearchSettings::OnlyThisDay);
    ASSERT_FALSE(result[0].has_value());
}


TEST_F(WeekScheduleRepeatingDayTest, FindCurrentTimePointLastPointOfTodayAllDays) {
    auto result = weekSchedule.findCurrentEventStatus(timePoints[0] - 20s,
                                                   WeekDay::monday, DaySearchSettings::AllDays);
    ASSERT_TRUE(result[0].has_value());
    // Previous day
    EXPECT_EQ(result[0]->eventData, channelData[2]);
}

TEST_F(WeekScheduleRepeatingDayTest, FindNextTimePointLastPointOfTodayAllDays) {
    auto result = weekSchedule.findNextEventStatus(timePoints[2] + 20s,
                                                   WeekDay::monday, DaySearchSettings::AllDays);
    ASSERT_TRUE(result[0].has_value());
    // Next day
    EXPECT_EQ(result[0]->eventData, channelData[0]);
}

TEST_F(WeekScheduleRepeatingDayTest, FindCurrentTimePointOtherDay) {
    auto result = weekSchedule.findCurrentEventStatus(5000s,
                                                     WeekDay::tuesday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 1);

    result = weekSchedule.findCurrentEventStatus(20000s, WeekDay::tuesday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 3);
}

TEST_F(WeekScheduleRepeatingDayTest, FindNextTimePointOtherDay) {
    auto result = weekSchedule.findNextEventStatus(5000s,
                                                   WeekDay::tuesday, DaySearchSettings::OnlyThisDay);
    ASSERT_TRUE(result[0].has_value());
    EXPECT_EQ(result[0]->eventData, 2);

    result = weekSchedule.findNextEventStatus(20000s, WeekDay::tuesday, DaySearchSettings::OnlyThisDay);
    EXPECT_FALSE(std::ranges::all_of(result, [](const auto &channelValue) { return channelValue.has_value(); }));
}