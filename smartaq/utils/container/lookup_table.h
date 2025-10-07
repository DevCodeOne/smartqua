#pragma once

#include <array>
#include <optional>
#include <ranges>
#include <mutex>
#include <shared_mutex>

#include "utils/conditional_thread_safety.h"

template<typename Instance, typename IndexType, typename ValueType>
class FlagTracker {
public:
    FlagTracker() = default;
    FlagTracker(IndexType index, ValueType resetTo, Instance *instance)
        : mIndex(index), mResetTo(resetTo), mResourceHolder(instance) {}
    FlagTracker(FlagTracker &&) = default;
    FlagTracker &operator=(FlagTracker &&) = default;

    FlagTracker(const FlagTracker&) = delete;
    FlagTracker &operator=(const FlagTracker&) = delete;

    explicit operator bool() const {
        return mIndex.has_value();
    }

    ~FlagTracker() {
        if (mResourceHolder && mIndex) {
            mResourceHolder->setIf(*mIndex, mResetTo);
        }
    }
private:
    std::optional<IndexType> mIndex{std::nullopt};
    ValueType mResetTo{};
    Instance *mResourceHolder{nullptr};
};

template<ThreadSafety Safety, auto ... ListOfValues>
struct ResourceLookupTable {
    using CommonType = std::common_type_t<decltype(ListOfValues) ...>;
    using FlagTrackerType =  FlagTracker<ResourceLookupTable, CommonType, bool>;

    static constexpr std::array<CommonType, sizeof...(ListOfValues)> indices{ListOfValues... };
    std::array<bool, sizeof...(ListOfValues)> values{false};

    FlagTrackerType setIfValue(CommonType Value, bool newValue, bool oldValue) {
        [[maybe_unused]] auto uniqueLock = mMutualExclusion.createUniqueLock();
        auto index = std::ranges::find(indices, Value);

        if (index == indices.cend()) {
            return FlagTrackerType{};
        }

        auto &currentValue = values[std::distance(indices.cbegin(), index)];
        if (currentValue != oldValue) {
            return FlagTrackerType{};
        }

        currentValue = newValue;
        return FlagTrackerType(Value, oldValue, this);
    }

     FlagTrackerType setIf(CommonType Value, bool newValue) {
        [[maybe_unused]] auto uniqueLock = mMutualExclusion.createUniqueLock();
        auto index = std::ranges::find(indices, Value);

        if (index == indices.cend()) {
            return FlagTrackerType{};
        }

        auto &currentValue = values[std::distance(indices.cbegin(), index)];

        currentValue = newValue;
        return FlagTrackerType{};
    }

    ConditionalThreadSafety<Safety> mMutualExclusion;
};