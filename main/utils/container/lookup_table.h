#pragma once

#include <array>
#include <optional>
#include <ranges>
#include <mutex>
#include <shared_mutex>

enum struct ThreadSafety {
    Safe, Unsafe
};

template<ThreadSafety Safety>
class ConditionalThreadSafety;

struct DoNothing {};

template<>
class ConditionalThreadSafety<ThreadSafety::Unsafe> {
public:
    auto createSharedLock() { return DoNothing{}; }
    auto createUniqueLock() { return DoNothing{}; }
};

template<>
class ConditionalThreadSafety<ThreadSafety::Safe> {
public:
    auto createSharedLock() { return std::shared_lock{mutex}; }
    auto createUniqueLock() { return std::unique_lock{mutex}; }

    std::shared_mutex mutex;
};


template<ThreadSafety Safety>
class ConditionalThreadSafety;

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
    using CommonType = decltype((ListOfValues , ...));
    using FlagTrackerType =  FlagTracker<ResourceLookupTable, CommonType, bool>;

    static constexpr std::array<CommonType, sizeof...(ListOfValues)> indices{ListOfValues... };
    std::array<bool, sizeof...(ListOfValues)> values{false};

    FlagTrackerType setIfValue(CommonType Value, bool newValue, bool oldValue) {
        [[maybe_unused]] auto uniqueLock = mMutialExclusion.createUniqueLock();
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
        [[maybe_unused]] auto uniqueLock = mMutialExclusion.createUniqueLock();
        auto index = std::ranges::find(indices, Value);

        if (index == indices.cend()) {
            return FlagTrackerType{};
        }

        auto &currentValue = values[std::distance(indices.cbegin(), index)];

        currentValue = newValue;
        return FlagTrackerType{};
    }

    ConditionalThreadSafety<Safety> mMutialExclusion;
};