#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <utility>

template<uint8_t NumChannels, typename Datapoint, uint8_t TimePointsPerDay>
class DaySchedule {
public:
    using ChannelData = std::array<std::optional<Datapoint>, NumChannels>;
    using TimePointData = std::pair<std::chrono::seconds, ChannelData>;
    using TimePointDataArray = std::array<TimePointData, TimePointsPerDay>;

    static constexpr std::chrono::minutes InvalidTime = std::chrono::hours(24) + std::chrono::minutes(1);

    DaySchedule() {
        for (auto &[currentTimeOnDay, currentData] : datapoints) {
            currentTimeOnDay = InvalidTime;
        }
    }

    template<typename DurationType>
    auto findSlotWithTime(const DurationType &timeOfDay) {
        return std::ranges::find_if(datapoints, [&timeOfDay](auto &currentPair) {
            return currentPair.first == timeOfDay;
        });
    }

    template<typename DurationType>
    auto findSlotWithTime(const DurationType &timeOfDay) const {
        return std::ranges::find_if(datapoints, [&timeOfDay](auto &currentPair) {
            return currentPair.first == timeOfDay;
        });
    }

    void reorderSchedule() {
        std::ranges::sort(datapoints, [](const auto &lhs, const auto &rhs) {
            return lhs.first < rhs.first;
        });
    }

    template<typename DurationType>
    bool insertTimePoint(const DurationType &eventAt, const ChannelData &data) {
        auto foundSlot = findSlotWithTime(InvalidTime);

        if (foundSlot == datapoints.cend()) {
            return false;
        }

        if (const auto hasTimePointAlready = findSlotWithTime(eventAt);
            hasTimePointAlready != datapoints.cend()) {
            return false;
        }

        foundSlot->first = eventAt;
        foundSlot->second = data;
        reorderSchedule();
        return true;
    }

    template<typename DurationType>
    bool removeTimePoint(const DurationType &eventAt) {
        auto foundSlot = findSlotWithTime(eventAt);

        if (foundSlot == datapoints.cend()) {
            return false;
        }

        foundSlot->first = InvalidTime;
        reorderSchedule();
        return true;
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

        if (foundSlot == datapoints.crend() || foundSlot->first == InvalidTime) {
            return datapoints.cend();
        }

        return (foundSlot.base() - 1);
    }

    template<typename DurationType = std::chrono::seconds>
    auto getNextTimePointOfDay(const DurationType &timeOfDay) const {
        // Find slot, which is the first to be later in the day
        auto foundSlot = std::ranges::find_if(datapoints,
        [&timeOfDay](const auto &currentPair) {
            return currentPair.first > timeOfDay;
        });

        if (foundSlot == datapoints.cend() || foundSlot->first == InvalidTime) {
            return datapoints.cend();
        }

        return foundSlot;
    }

    auto begin() const { return datapoints.cbegin(); }
    auto end() const { return datapoints.cend(); }

private:
    TimePointDataArray datapoints;
};
