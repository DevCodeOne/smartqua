#pragma once

#include <array>
#include <optional>
#include <cstdint>

#include "utils/time/time_utils.h"
#include "utils/time/day_schedule.h"

enum struct DaySearchSettings {
    OnlyThisDay, AllDays
};

template<typename TimePointData>
struct SingleChannelStatus {
    std::chrono::seconds eventTime;
    TimePointData eventData;
};

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
class WeekSchedule {
public:
    static constexpr uint8_t Channels = NumChannels;

    using DayScheduleType = DaySchedule<NumChannels, TimePointData, TimePointsPerDay>;
    using DayScheduleArrayType = std::array<DayScheduleType, 7>;
    using TimePointInfoType = typename DayScheduleType::TimePointData;

    using MultiChannelStatus = std::array<std::optional<SingleChannelStatus<TimePointData>>, NumChannels>;

    struct CurrentMultiChannelStatus {
        MultiChannelStatus current;
        MultiChannelStatus next;
    };

    WeekSchedule() = default;
    explicit WeekSchedule(const DayScheduleArrayType &schedule) {
        for (auto currentDay = 0; currentDay < schedule.size(); ++currentDay) {
            setDaySchedule(static_cast<weekday>(currentDay), schedule[currentDay]);
        }
    }

    template<typename DurationType>
    MultiChannelStatus findCurrentEventStatus(const DurationType &unitThisDay, DaySearchSettings settings = DaySearchSettings::AllDays,
        std::optional<weekday> day = std::nullopt) const;

    template<typename DurationType>
    MultiChannelStatus findNextEventStatus(const DurationType &unitThisDay, DaySearchSettings settings = DaySearchSettings::AllDays,
        std::optional<weekday> day = std::nullopt) const;

    template<typename DurationType>
    CurrentMultiChannelStatus currentEventStatus(const DurationType &unitThisDay, DaySearchSettings settings = DaySearchSettings::AllDays,
        std::optional<weekday> day = std::nullopt) const {
        return {
            .current = findCurrentEventStatus(unitThisDay, settings, day),
            .next = findNextEventStatus(unitThisDay, settings, day)
        };
    }

    auto &setDaySchedule(weekday day, const DayScheduleType &daySchedule) {
        daySchedules[static_cast<uint32_t>(day)] = daySchedule;
        return *this;
    }

private:
    DayScheduleArrayType daySchedules;

    template<typename TimeUnit>
    static SingleChannelStatus<TimePointData> createSingleChannelStatus(
        const auto dayIndex, const std::pair<TimeUnit, TimePointData> &timePointDay) {
        using std::chrono::hours;
        return SingleChannelStatus<TimePointData>{
            .eventTime = std::chrono::seconds{hours{dayIndex * 24} + timePointDay.first},
            .eventData = timePointDay.second
        };
    }
};

// TODO: Rework this into one method
template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::findCurrentEventStatus(const DurationType &unitThisDay, DaySearchSettings settings, std::optional<weekday> day) const -> MultiChannelStatus {
    // If if has no value take current day
    MultiChannelStatus status;

    if (!day.has_value()) {
        day = getDayOfWeek();
    }

    for (uint8_t currentChannel = 0; currentChannel < NumChannels; ++currentChannel) {
        auto dayIndex = static_cast<uint32_t>(*day);
        auto result = daySchedules[dayIndex].getCurrentTimePointOfDay(currentChannel, unitThisDay);

        if (settings != DaySearchSettings::AllDays) {
            if (!result.has_value()) {
                continue;
            }

            status[currentChannel] = createSingleChannelStatus(dayIndex, *result);
        }

        // If nothing was found, find the first available timepoint
        for (uint8_t daysSearched = 0; !result.has_value() && daysSearched < 8; ++daysSearched) {
            dayIndex = static_cast<uint32_t>(getPreviousDay(static_cast<weekday>(dayIndex)));
            result = daySchedules[dayIndex].getLastTimePointOfDay(currentChannel);
            ++daysSearched;
        }

        if (result.has_value()) {
            status[currentChannel] = createSingleChannelStatus(dayIndex, *result);
        }

    }

    return status;
}

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::findNextEventStatus(const DurationType &unitThisDay, DaySearchSettings settings, std::optional<weekday> day) const -> MultiChannelStatus {
    // If if has no value use current day
    MultiChannelStatus status;

    if (!day.has_value()) {
        day = getDayOfWeek();
    }

    for (unsigned int currentChannel = 0; currentChannel < NumChannels; ++currentChannel) {
        auto dayIndex = static_cast<uint32_t>(*day);
        auto result = daySchedules[dayIndex].getNextTimePointOfDay(currentChannel, unitThisDay);

        if (settings != DaySearchSettings::AllDays) {
            if (!result.has_value()) {
                continue;
            }

            status[currentChannel] = createSingleChannelStatus(dayIndex, *result);
        }

        // If nothing was found, find the first available timepoint
        for (uint8_t daysSearched = 0; !result.has_value() && daysSearched < 8; ++daysSearched) {
            dayIndex = static_cast<uint32_t>(getNextDay(static_cast<weekday>(dayIndex)));
            result = daySchedules[dayIndex].getFirstTimePointOfDay(currentChannel);
            ++daysSearched;
        }

        if (result.has_value()) {
            status[currentChannel] = createSingleChannelStatus(dayIndex, *result);
        }
    }

    return status;
}