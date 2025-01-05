#pragma once

#include <array>
#include <optional>
#include <cstdint>

#include "utils/time/time_utils.h"
#include "utils/time/day_schedule.h"

#include "build_config.h"
#include "utils/logger.h"

enum struct DaySearchSettings {
    OnlyThisDay, AllDays
};

namespace Detail {
    template<typename TimePointData>
    struct SingleChannelStatus {
        std::chrono::seconds eventTime;
        TimePointData eventData;
    };
}

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
class WeekSchedule {

    enum struct EventSelection {
        Next, Current
    };

public:
    static constexpr uint8_t Channels = NumChannels;

    using DayScheduleType = DaySchedule<NumChannels, TimePointData, TimePointsPerDay>;
    using DayScheduleArrayType = std::array<DayScheduleType, 7>;
    using TimePointInfoType = typename DayScheduleType::TimePointData;

    using SingleChannelStatus = Detail::SingleChannelStatus<TimePointData>;
    using OptionalSingleChannelStatus = std::optional<SingleChannelStatus>;
    using MultiChannelStatus = std::array<OptionalSingleChannelStatus, NumChannels>;

    struct CurrentMultiChannelStatus {
        MultiChannelStatus current;
        MultiChannelStatus next;
    };

    WeekSchedule() = default;
    explicit WeekSchedule(const DayScheduleArrayType &schedule) {
        for (auto currentDay = 0; currentDay < schedule.size(); ++currentDay) {
            setDaySchedule(static_cast<WeekDay>(currentDay), schedule[currentDay]);
        }
    }

    template<typename DurationType>
    MultiChannelStatus findCurrentEventStatus(const DurationType &unitThisDay, WeekDay day,
                                              DaySearchSettings settings = DaySearchSettings::AllDays) const;

    template<typename DurationType>
    MultiChannelStatus findNextEventStatus(const DurationType &unitThisDay, WeekDay day,
                                           DaySearchSettings settings = DaySearchSettings::AllDays) const;

    template<typename DurationType>
    CurrentMultiChannelStatus currentEventStatus(const DurationType &unitThisDay, WeekDay day,
                                                 DaySearchSettings settings = DaySearchSettings::AllDays) const {
        return {
            .current = findCurrentEventStatus(unitThisDay, day, settings),
            .next = findNextEventStatus(unitThisDay, day, settings)
        };
    }

    auto &setDaySchedule(WeekDay day, const DayScheduleType &daySchedule) {
        const auto dayIndex = static_cast<uint32_t>(day);

        if (dayIndex >= daySchedules.size()) {
            return *this;
        }

        daySchedules[dayIndex] = daySchedule;
        return *this;
    }

private:
    DayScheduleArrayType daySchedules;

    template<typename TimeUnit>
    static SingleChannelStatus createSingleChannelStatus(
        const auto dayIndex, const std::pair<TimeUnit, TimePointData> &timePointDay) {
        using std::chrono::hours;
        return SingleChannelStatus{
            // TODO: Probably will cause bugs e.g. saturday to sunday offset will be negative
            .eventTime = std::chrono::seconds{hours{dayIndex * 24} + timePointDay.first},
            .eventData = timePointDay.second
        };
    }

    template<typename DurationType>
    MultiChannelStatus findEventStatus(EventSelection selection, const DurationType &unitThisDay, WeekDay day,
                                           DaySearchSettings settings = DaySearchSettings::AllDays) const;

};

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::findCurrentEventStatus(const DurationType &unitThisDay, WeekDay day, DaySearchSettings settings) const -> MultiChannelStatus {
    return findEventStatus(EventSelection::Current, unitThisDay, day, settings);
}

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
auto WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::findNextEventStatus(const DurationType &unitThisDay, WeekDay day, DaySearchSettings settings) const -> MultiChannelStatus {
    return findEventStatus(EventSelection::Next, unitThisDay, day, settings);
}

template<uint8_t NumChannels, typename TimePointData, uint8_t TimePointsPerDay>
template<typename DurationType>
typename WeekSchedule<NumChannels, TimePointData, TimePointsPerDay>::MultiChannelStatus WeekSchedule<NumChannels,
TimePointData, TimePointsPerDay>::findEventStatus(EventSelection selection, const DurationType &unitThisDay,
    WeekDay day, DaySearchSettings settings) const {
    // If if has no value use current day
    MultiChannelStatus status{};

    const auto startDay = static_cast<uint32_t>(day);

    if (startDay >= daySchedules.size()) {
        return status;
    }

    for (unsigned int currentChannel = 0; currentChannel < NumChannels; ++currentChannel) {
        auto dayIndex = startDay;
        typename DayScheduleType::ChannelEventResult result{ std::nullopt };
        if (selection == EventSelection::Next) {
            result = daySchedules[dayIndex].getNextTimePointOfDay(currentChannel, unitThisDay);
        } else {
            result = daySchedules[dayIndex].getCurrentTimePointOfDay(currentChannel, unitThisDay);
        }

        if (settings != DaySearchSettings::AllDays) {
            if (!result.has_value()) {
                continue;
            }

            status[currentChannel] = createSingleChannelStatus(dayIndex, *result);
        }

        // If nothing was found, find the first available timepoint
        for (uint8_t daysSearched = 0; !result.has_value() && daysSearched < 8; ++daysSearched) {
            if (selection == EventSelection::Next) {
                dayIndex = static_cast<uint32_t>(getNextDay(static_cast<WeekDay>(dayIndex)));
                result = daySchedules[dayIndex].getFirstTimePointOfDay(currentChannel);
            } else {
                dayIndex = static_cast<uint32_t>(getPreviousDay(static_cast<WeekDay>(dayIndex)));
                result = daySchedules[dayIndex].getLastTimePointOfDay(currentChannel);
            }
            ++daysSearched;
        }

        if (result.has_value()) {
            Logger::log(LogLevel::Debug, "Found event for channel %d", currentChannel);
            status[currentChannel] = createSingleChannelStatus(dayIndex, *result);
        } else {
            Logger::log(LogLevel::Debug, "No event found for channel %d", currentChannel);
        }
    }

    return status;
}
