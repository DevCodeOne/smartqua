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

    static inline constexpr auto InvalidTime = std::chrono::hours(24) + std::chrono::seconds(1);

    DaySchedule() {
        for (auto &[currentTimeOnDay, currentData] : datapoints) {
            currentTimeOnDay = InvalidTime;
        }
    }

    auto findSlotWithTime(const std::chrono::seconds &timeOfDay) {
        return std::find_if(datapoints.begin(), datapoints.end(), [&timeOfDay](auto &currentPair) {
            return currentPair.first == timeOfDay;
        });
    }

    auto findSlotWithTime(const std::chrono::seconds &timeOfDay) const {
        return std::find_if(datapoints.cbegin(), datapoints.cend(), [&timeOfDay](auto &currentPair) {
            return currentPair.first == timeOfDay;
        });
    }

    void reorderSchedule() {
        std::sort(datapoints.begin(), datapoints.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.first < rhs.first;
        });
    }

    bool insertTimePoint(const std::pair<std::chrono::seconds, TimePointData> &data) {
        auto foundSlot = findSlotWithTime(InvalidTime);

        if (foundSlot != datapoints.cend()) {
            *foundSlot = data;
            reorderSchedule();
            return true;
        }

        return false;
    }

    bool removeTimePoint(const std::pair<std::chrono::seconds, TimePointData> &data) {
        auto foundSlot = findSlotWithTime(data.first);

        if (foundSlot != datapoints.cend()) {
            foundSlot->first = InvalidTime;
            reorderSchedule();
            return true;
        }

        return false;
    }

    auto getFirstTimePointOfDay() const { 
        if (datapoints.begin()->first != InvalidTime) {
            return datapoints.cbegin(); 
        }
        return datapoints.cend();
    }

    // Return last point before the invalid timepoint section begins
    auto getLastTimePointOfDay() const { 
        auto foundSlot = findSlotWithTime(InvalidTime);
        if (foundSlot == datapoints.begin()) {
            return datapoints.end();
        }

        return foundSlot - 1; 
    }

    template<typename DurationType = std::chrono::seconds>
    auto getCurrentTimePointOfDay(const DurationType &timeOfDay) const { 
        auto foundSlot = getNextTimePointOfDay(timeOfDay);

        if (foundSlot == datapoints.cend()) {
            return datapoints.cend();
        }

        // There is no earlier datapoint than this one -> return invalid marker
        if (foundSlot == datapoints.cbegin()) {
            return datapoints.cend();
        }

        // Otherwise return the previous data slot
        return foundSlot - 1;
    }

    template<typename DurationType = std::chrono::seconds>
    auto getNextTimePointOfDay(const DurationType &timeOfDay) const { 
        // Find slot, which is the first to be later in the day
        auto foundSlot = std::find_if(datapoints.cbegin(), datapoints.cend(), [&timeOfDay](const auto &currentPair) {
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
    auto end() const { return datapoints.cbegin(); }

private:
    TimePointArrayType datapoints;
};


template<typename TimePointData, uint8_t TimePointsPerDay>
class WeekSchedule {
    using DayScheduleType = DaySchedule<TimePointData, TimePointsPerDay>;
    using DayScheduleArrayType = std::array<DayScheduleType, 7>;

    template<typename DurationType = std::chrono::seconds>
    auto findCurrentTimePoint(const DurationType &unitThisDay, std::optional<weekday> day = std::nullopt) const {
        // If if has no value take current day
        if (!day.has_value()) {
            day = getDayOfWeek();
        }

        uint32_t dayIndex = static_cast<uint32_t>(*day);

        auto result = daySchedules[dayIndex].getCurrentTimePointOfDay(unitThisDay);

        // If nothing was fount find the first available timepoint
        while (result == daySchedules[dayIndex].end()) {
            dayIndex = static_cast<uint32_t>(getPreviousDay(static_cast<weekday>(dayIndex)));
            result = daySchedules[dayIndex].getLastTimePointOfDay();
        }

        return *result;
    }

    template<typename DurationType>
    auto findNextTimePoint(const DurationType &unitThisDay, std::optional<weekday> day = std::nullopt) const {
        // If if has no value take current day
        if (!day.has_value()) {
            day = getDayOfWeek();
        }

        uint32_t dayIndex = static_cast<uint32_t>(*day);

        auto result = daySchedules[dayIndex].getNextTimePointOfDay(unitThisDay);

        // If nothing was fount find the first available timepoint
        while (result == daySchedules[dayIndex].end()) {
            dayIndex = static_cast<uint32_t>(getNextDay(static_cast<weekday>(dayIndex)));
            result = daySchedules[dayIndex].getFirstTimePointOfDay();
        }

        return *result;
    }

private:
    DayScheduleArrayType daySchedules;
};