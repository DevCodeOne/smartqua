#pragma once

#include <optional>
#include <cstdint>

#include "utils/logger.h"

// Tracker type enum
enum struct ScheduleEventTransitionMode {
    Interpolation,
    SingleShot,
    Hold
};

// TODO: Add concept for eventOffset
template<typename EventType, typename MinimalTimeUnit>
struct TrackingData {
    EventType current;
    std::optional<EventType> next;
    MinimalTimeUnit channelTime;
    MinimalTimeUnit currentEventInEffectSince;
    MinimalTimeUnit now;
};

// Hold Tracker
template<typename ValueType>
struct HoldTracker {
    template<typename EventType, typename MinimalTimeUnit>
    std::optional<ValueType> getChannelValue(const TrackingData<EventType, MinimalTimeUnit> &trackingData) const {
        Logger::log(LogLevel::Debug, "Holdtracker: Current event data is: %f", trackingData.current.eventData);
        return { trackingData.current.eventData };
    }
};

// Interpolation Tracker
template<typename ValueType>
struct InterpolationTracker {
    template<typename EventType, typename MinimalTimeUnit>
    std::optional<ValueType> getChannelValue(const TrackingData<EventType, MinimalTimeUnit> &trackingData) const {
        const auto &currentEvent = trackingData.current;
        const auto &nextEvent = trackingData.next;

        if (!nextEvent) {
            return {};
        }

        Logger::log(LogLevel::Debug, "InterpolationTracker: Current event data is: %d, next event data is : %d", (int) (currentEvent.eventData * 100),
                    (int) (nextEvent->eventData * 100));

        // TODO: Fix negative values
        const auto currentEventInEffectSince = std::chrono::duration_cast<std::chrono::seconds>(
            trackingData.currentEventInEffectSince).count();

        if (currentEventInEffectSince == 0) {
            return {ValueType{currentEvent.eventData}};
        }

        const auto difference = nextEvent->eventTime - currentEvent.eventTime;
        const auto valueDifference = nextEvent->eventData - currentEvent.eventData;
        const auto interpolationFactor = valueDifference
                                         / static_cast<float>(std::chrono::duration_cast<std::chrono::seconds>(
                                             difference).count());

        return {
            static_cast<ValueType>(
                interpolationFactor * currentEventInEffectSince +
                currentEvent.eventData
            )
        };
    }
};

// Single Shot Tracker
template<typename ValueType>
struct SingleShotTracker {
    template<typename EventType, typename MinimalTimeUnit>
    std::optional<ValueType> getChannelValue(const TrackingData<EventType, MinimalTimeUnit> &trackingData) const {
        Logger::log(LogLevel::Debug, "SingleShotTracker: Current event data is: %f", trackingData.current.eventData);
        // If negative, it means that now is earlier in the week than eventTime
        auto &currentEvent = trackingData.current;
        const bool channelTimeIsOfLastWeek = trackingData.channelTime > trackingData.now;
        const bool eventIsOfLastWeek = currentEvent.eventTime > trackingData.now;

        // Times are in the same range, do easy comparison
        if ((channelTimeIsOfLastWeek == eventIsOfLastWeek)
            && trackingData.channelTime < currentEvent.eventTime) {
            return {currentEvent.eventData};
        }

        // ChannelTime is of last week, event is of this week, trigger this event without comparison
        if (channelTimeIsOfLastWeek && not eventIsOfLastWeek) {
            return {currentEvent.eventData};
        }

        // not channelTimeIsOfLastWeek && eventIsOfLastWeek -> channelTime is already later with this
        // so no event should be triggered

        return {};
    }
};
