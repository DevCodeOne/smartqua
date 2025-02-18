#pragma once

#include <optional>
#include <array>
#include <chrono>
#include <variant>

#include "utils/time/schedule.h"
#include "utils/time/schedule_tracker_types.h"
#include "utils/time/time_utils.h"
#include "utils/logger.h"
#include "build_config.h"

template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
class ScheduleTracker final {
public:
    using MinimalTimeUnit = std::chrono::seconds;
    using UnderlyingScheduleType = ScheduleType;
    using OptionalChannelValue = std::optional<ValueType>;
    using OptionalChannelValues = std::array<OptionalChannelValue, NumChannels>;
    using TrackerTypeVariant = std::variant<
        InterpolationTracker<ValueType>,
        SingleShotTracker<ValueType>,
        HoldTracker<ValueType> >;

    ScheduleTracker(const ScheduleType *schedule, ScheduleEventTransitionMode type) : mTrackerType(createTrackingType(type)),
                                                                              mSchedule(schedule) {
    }
    explicit ScheduleTracker(const ScheduleType *schedule) : ScheduleTracker(schedule, ScheduleEventTransitionMode::Hold) {
    }
    ScheduleTracker(const ScheduleTracker &) = delete;
    ScheduleTracker(ScheduleTracker &&) = default;
    ScheduleTracker &operator=(const ScheduleTracker &) = delete;
    ScheduleTracker &operator=(ScheduleTracker &&) = default;
    ~ScheduleTracker() = default;

    OptionalChannelValues getCurrentChannelValues(const std::tm &currentDate) const;
    OptionalChannelValue getCurrentChannelValue(uint8_t channelIndex, const std::tm &currentDate) const;

    void setTrackingType(ScheduleEventTransitionMode trackerType);

    void updateAllChannelTimes(const std::tm &currentDate);
    bool updateChannelTime(uint8_t channelIndex, const std::tm &currentDate);

    void setSchedule(const ScheduleType *schedule) {
        mSchedule = schedule;
    }

    const std::array<MinimalTimeUnit, NumChannels> &getChannelTimes() const {
        return mChannelTimes;
    }

    // TODO: Maybe add tests, so only valid times are set?
    void setChannelTimes(std::array<MinimalTimeUnit, NumChannels> &channelTimes) {
        std::copy(channelTimes.begin(), channelTimes.end(), mChannelTimes.begin());
    }

private:
    std::array<MinimalTimeUnit, NumChannels> mChannelTimes;
    TrackerTypeVariant mTrackerType;

    const ScheduleType *mSchedule;

    enum struct EventSelection {
        Next, Current
    };

    typename ScheduleType::OptionalSingleChannelStatus getEvent(EventSelection select, uint8_t channelIndex, const WeekDay &dayInWeek, const MinimalTimeUnit &timeToday) const;
    static TrackerTypeVariant createTrackingType(ScheduleEventTransitionMode trackerType);
};

template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
auto ScheduleTracker<ScheduleType, ValueType, NumChannels>::getCurrentChannelValues(const std::tm &currentDate) const -> OptionalChannelValues {
    OptionalChannelValues values;
    for (uint8_t i = 0; i < NumChannels; ++i) {
        values[i] = getCurrentChannelValue(i, currentDate);
    }
    return values;
}

template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
auto ScheduleTracker<ScheduleType,
    ValueType, NumChannels>::getCurrentChannelValue(uint8_t channelIndex,
                                                    const std::tm &currentDate) const -> OptionalChannelValue {
    if (channelIndex >= NumChannels) {
        return {};
    }

    const auto timeSinceWeekBeginning = sinceWeekBeginning<std::chrono::seconds>(currentDate);
    const auto timeThisDay = getTimeOfDay<std::chrono::seconds>(currentDate);
    const auto dayInWeek = getDayOfWeek(currentDate);

    auto getEventData = [&](const EventSelection selection) {
        return getEvent(selection, channelIndex, dayInWeek, timeThisDay);
    };

    const auto currentEvent = getEventData(EventSelection::Current);
    const auto nextEvent = getEventData(EventSelection::Next);

    if (!currentEvent.has_value()) {
        Logger::log(LogLevel::Debug, "No current event for channel %d", channelIndex);
        return {};
    }

    const auto eventInEffectSince = timeSinceWeekBeginning - currentEvent->eventTime;

    const TrackingData<typename ScheduleType::SingleChannelStatus, std::chrono::seconds> trackerData{
        .current = *currentEvent,
        .next = nextEvent,
        .channelTime = mChannelTimes[channelIndex],
        .currentEventInEffectSince = eventInEffectSince,
        .now = timeSinceWeekBeginning
    };

    return std::visit([&trackerData](const auto &tracker) {
        return tracker.getChannelValue(trackerData);
    }, mTrackerType);
}

template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
auto ScheduleTracker<ScheduleType,
    ValueType, NumChannels>::createTrackingType(ScheduleEventTransitionMode trackerType) -> TrackerTypeVariant {
    switch (trackerType) {
        case ScheduleEventTransitionMode::Interpolation:
            Logger::log(LogLevel::Debug, "Using interpolation tracker");
            return InterpolationTracker<ValueType>{};
        case ScheduleEventTransitionMode::SingleShot:
            Logger::log(LogLevel::Debug, "Using single shot tracker");
            return SingleShotTracker<ValueType>{};
        case ScheduleEventTransitionMode::Hold:
            Logger::log(LogLevel::Debug, "Using hold tracker");
            return HoldTracker<ValueType>{};
        default:
            return {};
    }
}

template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
void ScheduleTracker<ScheduleType, ValueType, NumChannels>::setTrackingType(ScheduleEventTransitionMode trackerType) {
    mTrackerType = createTrackingType(trackerType);
}

template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
void ScheduleTracker<ScheduleType, ValueType, NumChannels>::updateAllChannelTimes(const std::tm &currentDate) {
    for (uint8_t i = 0; i < NumChannels; ++i) {
        updateChannelTime(i, currentDate);
    }
}

template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
bool ScheduleTracker<ScheduleType, ValueType, NumChannels>::updateChannelTime(uint8_t channelIndex,
                                                                              const std::tm &currentDate) {
    if (mChannelTimes.size() <= channelIndex) {
        return false;
    }

    mChannelTimes[channelIndex] = sinceWeekBeginning<std::chrono::seconds>(currentDate);
    return true;
}

// TODO: Only search for specified channel
template<typename ScheduleType, typename ValueType, uint8_t NumChannels>
auto ScheduleTracker<ScheduleType,
    ValueType, NumChannels>::getEvent(EventSelection selection, uint8_t channelIndex, const WeekDay &dayInWeek,
                                      const MinimalTimeUnit &timeToday) const -> typename ScheduleType::OptionalSingleChannelStatus {
    if (mSchedule == nullptr) {
        return {};
    }

    auto foundEvent = mSchedule->currentEventStatus(timeToday, dayInWeek, DaySearchSettings::AllDays);

    auto getEventData = [&channelIndex](const auto &event) -> typename ScheduleType::OptionalSingleChannelStatus {
        if (event.size() <= channelIndex) {
            return { };
        }

        auto &eventChannel = event[channelIndex];

        if (!eventChannel.has_value()) {
            return { };
        }

        return eventChannel;
    };

    if (selection == EventSelection::Next) {
        return getEventData(foundEvent.next);
    }
    if (selection == EventSelection::Current) {
        return getEventData(foundEvent.current);
    }

    return {};
}