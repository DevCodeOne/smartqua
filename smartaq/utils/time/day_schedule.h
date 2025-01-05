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

    using ChannelEventResult = std::optional<std::pair<std::chrono::seconds, Datapoint>>;

    static constexpr std::chrono::minutes InvalidTime = std::chrono::hours(24) + std::chrono::minutes(1);

    DaySchedule() {
        std::ranges::fill(datapoints, std::make_pair(InvalidTime, ChannelData{}));
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

    ChannelEventResult getFirstTimePointOfDay(uint8_t channelIndex) const {
        const auto firstEntry = std::ranges::find_if(datapoints, [channelIndex](const auto &currentPair) {
            return currentPair.first != InvalidTime && currentPair.second[channelIndex].has_value();
        });

        if (firstEntry == datapoints.cend()) {
            return {};
        }

        return { std::make_pair(firstEntry->first, *firstEntry->second[channelIndex]) };
    }

    // Return last point before the invalid timepoint section begins
    ChannelEventResult getLastTimePointOfDay(uint8_t channelIndex) const {
        const auto lastEntry = std::find_if(datapoints.rbegin(), datapoints.rend(),
                                            [channelIndex](const auto &currentPair) {
                                                return currentPair.first != InvalidTime && currentPair.second[
                                                           channelIndex].has_value();
                                            });

        if (lastEntry == datapoints.crend()) {
            return {};
        }

        const auto &value = (lastEntry.base() - 1);

        return { std::make_pair(value->first, *value->second[channelIndex]) };
    }

    template<typename DurationType = std::chrono::seconds>
    ChannelEventResult getCurrentTimePointOfDay(uint8_t channelIndex, const DurationType &timeOfDay) const {
        auto foundSlot = std::find_if(datapoints.crbegin(), datapoints.crend(),
                                      [&timeOfDay, channelIndex](const auto &currentPair) {
                                          return currentPair.first <= timeOfDay && currentPair.second[channelIndex].
                                                 has_value();
                                      });

        if (foundSlot == datapoints.crend() || foundSlot->first == InvalidTime) {
            return {};
        }

        const auto &value = (foundSlot.base() - 1);

        return {  std::make_pair(value->first, *value->second[channelIndex]) };
    }

    template<typename DurationType = std::chrono::seconds>
    ChannelEventResult getNextTimePointOfDay(uint8_t channelIndex, const DurationType &timeOfDay) const {
        // Find slot, which is the first to be later in the day
        auto foundSlot = std::ranges::find_if(datapoints,
        [&timeOfDay, channelIndex](const auto &currentPair) {
            return currentPair.first > timeOfDay && currentPair.second[channelIndex].has_value();
        });

        if (foundSlot == datapoints.cend() || foundSlot->first == InvalidTime) {
            return {};
        }

        return { std::make_pair(foundSlot->first, *foundSlot->second[channelIndex]) };
    }

    auto begin() const { return datapoints.cbegin(); }
    auto end() const { return datapoints.cend(); }

private:
    TimePointDataArray datapoints;

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


};
