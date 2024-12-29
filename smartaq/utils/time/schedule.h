#pragma once

#include <array>
#include <optional>
#include <cstdint>

#include "utils/time/time_utils.h"
#include "utils/time/day_schedule.h"

enum struct DaySearchSettings {
    OnlyThisDay, AllDays
};

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
class WeekSchedule {
public:
    static constexpr uint8_t Channels = NumChannels;

    using DayScheduleType = DaySchedule<NumChannels, TimePointData, TimePointsPerDay>;
    using DayScheduleArrayType = std::array<DayScheduleType, 7>;
    using TimePointInfoType = typename DayScheduleType::TimePointData;

    WeekSchedule() = default;
    explicit WeekSchedule(const DayScheduleArrayType &schedule);

    template<typename DurationType>
    std::optional<TimePointInfoType> findCurrentTimePoint(const DurationType &unitThisDay, DaySearchSettings settings = DaySearchSettings::AllDays,
        std::optional<weekday> day = std::nullopt) const;

    template<typename DurationType>
    std::optional<TimePointInfoType> findNextTimePoint(const DurationType &unitThisDay, DaySearchSettings settings = DaySearchSettings::AllDays,
        std::optional<weekday> day = std::nullopt) const;

    auto &setDaySchedule(weekday day, const DayScheduleType &daySchedule);

private:
    DayScheduleArrayType daySchedules;
};

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::WeekSchedule(const DayScheduleArrayType &schedule) {
    for (auto currentDay = 0; currentDay < schedule.size(); ++currentDay) {
        setDaySchedule(static_cast<weekday>(currentDay), schedule[currentDay]);
    }
}

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
auto &WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::setDaySchedule(weekday day, const DayScheduleType &daySchedule) {
        daySchedules[static_cast<uint32_t>(day)] = daySchedule;

        return *this;
}

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::findCurrentTimePoint(const DurationType &unitThisDay, DaySearchSettings settings, std::optional<weekday> day) const -> std::optional<TimePointInfoType> {
    // If if has no value take current day
    if (!day.has_value()) {
        day = getDayOfWeek();
    }

    const auto createResult = [](const auto dayIndex, const auto &timePointDay) {
        using std::chrono::hours;
        return TimePointInfoType{hours{dayIndex * 24} + timePointDay->first, timePointDay->second};
    };

    auto dayIndex = static_cast<uint32_t>(*day);
    auto result = daySchedules[dayIndex].getCurrentTimePointOfDay(unitThisDay);

    if (settings != DaySearchSettings::AllDays) {
        if (result == daySchedules[dayIndex].end()) {
            return {};
        }

        return createResult(dayIndex, result);
    }

    // If nothing was found, find the first available timepoint
    uint8_t daysSearched = 0;
    while (result == daySchedules[dayIndex].end() && daysSearched < 8) {
        dayIndex = static_cast<uint32_t>(getPreviousDay(static_cast<weekday>(dayIndex)));
        result = daySchedules[dayIndex].getLastTimePointOfDay();
        ++daysSearched;
    }

    if (daysSearched == 8 || result == daySchedules[dayIndex].end()) {
        return {};
    }

    return TimePointInfoType{ std::chrono::hours{dayIndex * 24} + result->first, result->second };
}

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::findNextTimePoint(const DurationType &unitThisDay, DaySearchSettings settings, std::optional<weekday> day) const -> std::optional<TimePointInfoType> {
    // If if has no value use current day
    if (!day.has_value()) {
        day = getDayOfWeek();
    }

    const auto createResult = [](const auto dayIndex, const auto &timePointDay) {
        using std::chrono::hours;
        return TimePointInfoType{hours{dayIndex * 24} + timePointDay->first, timePointDay->second};
    };

    auto dayIndex = static_cast<uint32_t>(*day);
    auto result = daySchedules[dayIndex].getNextTimePointOfDay(unitThisDay);

    if (settings != DaySearchSettings::AllDays) {
        if (result == daySchedules[dayIndex].end()) {
            return {};
        }

        return createResult(dayIndex, result);
    }

    // If nothing was found, find the first available timepoint
    uint8_t daysSearched = 0;
    while (result == daySchedules[dayIndex].end() && daysSearched < 8) {
        dayIndex = static_cast<uint32_t>(getNextDay(static_cast<weekday>(dayIndex)));
        result = daySchedules[dayIndex].getFirstTimePointOfDay();
        ++daysSearched;
    }

    if (daysSearched == 8 || result == daySchedules[dayIndex].end()) {
        return {};
    }

    return createResult(dayIndex, result);

}