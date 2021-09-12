#pragma once

#include <array>
#include <algorithm>
#include <optional>
#include <cstdint>

#include "utils/time_utils.h"

template<typename TimePointData, uint8_t TimePointsPerDay>
class DaySchedule {
public:
    using TimePointArrayType = std::array<std::pair<std::chrono::seconds, TimePointData>, TimePointsPerDay>;

    static inline constexpr std::chrono::minutes InvalidTime = std::chrono::hours(24) + std::chrono::minutes(1);

    DaySchedule() {
        for (auto &[currentTimeOnDay, currentData] : datapoints) {
            currentTimeOnDay = InvalidTime;
        }
    }

    template<typename DurationType>
    auto findSlotWithTime(const DurationType &timeOfDay) {
        return std::find_if(datapoints.begin(), datapoints.end(), [&timeOfDay](auto &currentPair) {
            return currentPair.first == timeOfDay;
        });
    }

    template<typename DurationType>
    auto findSlotWithTime(const DurationType &timeOfDay) const {
        return std::find_if(datapoints.cbegin(), datapoints.cend(), [&timeOfDay](auto &currentPair) {
            return currentPair.first == timeOfDay;
        });
    }

    void reorderSchedule() {
        std::sort(datapoints.begin(), datapoints.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.first < rhs.first;
        });
    }

    template<typename DurationType>
    bool insertTimePoint(const std::pair<DurationType, TimePointData> &data) {
        auto foundSlot = findSlotWithTime(InvalidTime);

        if (foundSlot == datapoints.cend()) {
            return false;
        }

        foundSlot->first = data.first;
        foundSlot->second = data.second;
        reorderSchedule();
        return true;
    }

    template<typename DurationType>
    bool removeTimePoint(const std::pair<DurationType, TimePointData> &data) {
        auto foundSlot = findSlotWithTime(data.first);

        if (foundSlot != datapoints.cend()) {
            foundSlot->first = InvalidTime;
            reorderSchedule();
            return true;
        }

        return false;
    }

    auto getFirstTimePointOfDay() const { 
        if (datapoints.cbegin()->first != InvalidTime) {
            return datapoints.cbegin(); 
        }
        return datapoints.cend();
    }

    // Return last point before the invalid timepoint section begins
    auto getLastTimePointOfDay() const { 
        auto foundSlot = findSlotWithTime(InvalidTime);
        if (foundSlot == datapoints.cbegin()) {
            return datapoints.cend();
        }
        return (foundSlot - 1); 
    }

    template<typename DurationType = std::chrono::seconds>
    auto getCurrentTimePointOfDay(const DurationType &timeOfDay) const { 
        auto foundSlot = std::find_if(datapoints.crbegin(), datapoints.crend(), 
        [&timeOfDay](const auto &currentPair) {
            return currentPair.first <= timeOfDay;
        });

        if (foundSlot == datapoints.crend()) {
            return datapoints.cend();
        }

        if (foundSlot->first == InvalidTime) {
            return datapoints.cend();
        }

        return (foundSlot.base() - 1);
    }

    template<typename DurationType = std::chrono::seconds>
    auto getNextTimePointOfDay(const DurationType &timeOfDay) const { 
        // Find slot, which is the first to be later in the day
        auto foundSlot = std::find_if(datapoints.cbegin(), datapoints.cend(), 
        [&timeOfDay](const auto &currentPair) {
            return currentPair.first > timeOfDay;
        });

        if (foundSlot == datapoints.cend()) {
            return datapoints.cend();
        }

        if (foundSlot->first == InvalidTime) {
            return datapoints.cend();
        }

        return foundSlot;
    }

    auto begin() const { return datapoints.cbegin(); }
    auto end() const { return datapoints.cend(); }

private:
    TimePointArrayType datapoints;
};


template<typename TimePointData, uint8_t TimePointsPerDay>
class WeekSchedule {
public:
    using DayScheduleType = DaySchedule<TimePointData, TimePointsPerDay>;
    using DayScheduleArrayType = std::array<DayScheduleType, 7>;
    using TimePointDataType = TimePointData;

    WeekSchedule() = default;
    WeekSchedule(const DayScheduleArrayType &schedule);

    template<typename DurationType>
    auto findCurrentTimePoint(const DurationType &unitThisDay, std::optional<weekday> day = std::nullopt) const;

    template<typename DurationType>
    auto findNextTimePoint(const DurationType &unitThisDay, std::optional<weekday> day = std::nullopt) const;

    auto &setDaySchedule(weekday day, const DayScheduleType &daySchedule);

private:
    DayScheduleArrayType daySchedules;
};

template<typename TimePointData, uint8_t TimePointsPerDay>
auto &WeekSchedule<TimePointData, TimePointsPerDay>::setDaySchedule(weekday day, const DayScheduleType &daySchedule) {
        daySchedules[static_cast<uint32_t>(day)] = daySchedule;

        return *this;
}

// TODO: return timepoint on day + day in seconds

template<typename TimePointData, typename DurationType = std::chrono::seconds>
using TimePointInfoType = std::pair<DurationType, TimePointData>;

template<typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<TimePointData, TimePointsPerDay>::findCurrentTimePoint(const DurationType &unitThisDay, std::optional<weekday> day) const {
    // If if has no value take current day
    if (!day.has_value()) {
        day = getDayOfWeek();
    }

    uint32_t dayIndex = static_cast<uint32_t>(*day);
    auto result = daySchedules[dayIndex].getCurrentTimePointOfDay(unitThisDay);

    // If nothing was fount find the first available timepoint
    uint8_t daysSearched = 0;
    while (result == daySchedules[dayIndex].end() && daysSearched < 8) {
        dayIndex = static_cast<uint32_t>(getPreviousDay(static_cast<weekday>(dayIndex)));
        result = daySchedules[dayIndex].getLastTimePointOfDay();
        ++daysSearched;
    }

    if (daysSearched == 8) {
        return std::make_optional<TimePointInfoType<TimePointData>>();
    }

    return std::make_optional<TimePointInfoType<TimePointData>>(
        std::make_pair(std::chrono::hours{dayIndex * 24} + result->first, result->second));
}

template<typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<TimePointData, TimePointsPerDay>::findNextTimePoint(const DurationType &unitThisDay, std::optional<weekday> day) const {
    // If if has no value take current day
    if (!day.has_value()) {
        day = getDayOfWeek();
    }

    uint32_t dayIndex = static_cast<uint32_t>(*day);
    auto result = daySchedules[dayIndex].getNextTimePointOfDay(unitThisDay);

    // If nothing was found find the first available timepoint
    uint8_t daysSearched = 0;
    while (result == daySchedules[dayIndex].end() && daysSearched < 8) {
        dayIndex = static_cast<uint32_t>(getNextDay(static_cast<weekday>(dayIndex)));
        result = daySchedules[dayIndex].getFirstTimePointOfDay();
        ++daysSearched;
    }

    if (daysSearched == 8) {
        return std::make_optional<TimePointInfoType<TimePointData>>();
    }

    return std::make_optional<TimePointInfoType<TimePointData>>(
        std::make_pair(std::chrono::hours{dayIndex * 24} + result->first, result->second));

}