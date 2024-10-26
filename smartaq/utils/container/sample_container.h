#pragma once

#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <optional>
#include <cmath>
#include <ranges>

#include "ring_buffer.h"

template<typename T>
struct Sample {
    T value;
    std::chrono::time_point<std::chrono::steady_clock> timeStamp;
};

template <typename T, typename AvgType = T, uint32_t n_samples = 10u>
requires(n_samples >= 10)
class SampleContainer final {
   public:
    SampleContainer() = default;
    explicit SampleContainer(AvgType maxRateOfChange) : mVariance(0), mStdDerivation(0), mAvgRateOfChange(0),
                                                        mMaxRateOfChange(maxRateOfChange) {
    }

    SampleContainer(const SampleContainer &other) {
        std::scoped_lock other_instance(other.mResourceMutex);

        copyFrom(other);
    }

    SampleContainer(SampleContainer &&other) noexcept {
        std::scoped_lock lock(other.mResourceMutex);

        moveFrom(other);
    }

    ~SampleContainer() = default;

    SampleContainer &operator=(const SampleContainer &other) {
        if (this == &other) {
            return *this;
        }
        std::scoped_lock lock(mResourceMutex, other.mResourceMutex);

        copyFrom(other);
        return *this;
    }

    SampleContainer &operator=(SampleContainer &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        std::scoped_lock lock(mResourceMutex, other.mResourceMutex);

        moveFrom(other);
        return *this;
    }

    // Return if value was accepted
    bool putSample(T value) {
        return putSample(value, std::chrono::steady_clock::now());
    }

    // Overloaded method with additional timeStamp parameter
    bool putSample(T value, std::chrono::time_point<std::chrono::steady_clock> timeStamp) {
        std::unique_lock _instance_lock{mResourceMutex};

        if (!checkSampleViability(value, timeStamp)) {
            return false;
        }

        mSamples.append({value, timeStamp});
        recalculateInternalValues(_instance_lock);

        return true;
    }

    AvgType average() const {
        std::shared_lock _instance_lock{mResourceMutex};

        return mAvg;
    }

    AvgType last() const {
        std::shared_lock _instance_lock{mResourceMutex};

        return mSamples.back().value;
    }

    auto stdVariance() const {
        std::shared_lock _instance_lock{mResourceMutex};

        return mStdDerivation;
    }

    auto variance() const {
        std::shared_lock _instance_lock{mResourceMutex};

        return mVariance;
    }

    auto size() const {
        std::shared_lock _instance_lock{mResourceMutex};

        return mSamples.size();
    }

   private:
    static auto calculateTimeDifference(const auto &time1, const auto &time2) {
        return std::abs(std::chrono::duration_cast<std::chrono::seconds>(time1 - time2).count());
    }

    // TODO: maybe don't recalculate complete new average
    template<typename MutexType>
    SampleContainer &recalculateInternalValues(std::unique_lock<MutexType> &lock) {
        if (!lock.owns_lock()) {
            return *this;
        }

        if (mSamples.empty()) {
            return *this;
        }

        const auto divisor = 1.0f / mSamples.size();
        float summed_up_values = 0;

        for (typename decltype(mSamples)::SizeType i = 0; i < mSamples.size(); ++i) {
            summed_up_values += mSamples[i].value * divisor;
        }

        if constexpr (std::is_integral_v<AvgType>) {
            mAvg = static_cast<AvgType>(std::lround(summed_up_values));
        } else {
            mAvg = summed_up_values;
        }

        float ss = 0;
        for (typename decltype(mSamples)::SizeType i = 0; i < mSamples.size(); ++i) {
            ss += (mSamples[i].value - mAvg) * (mSamples[i].value - mAvg);
        }

        if (mSamples.size() <= 1) {
            mAvgRateOfChange = 0.0f;
            return *this;
        }

        const auto sampleDivisor = mSamples.size() - 1;
        mVariance = ss / sampleDivisor;
        mStdDerivation = std::sqrt(mVariance);

        // Calculate average rate of change
        float totalRateOfChange = 0.0f;
        for (typename decltype(mSamples)::SizeType i = 1; i < mSamples.size(); ++i) {
            const auto timeDiff = calculateTimeDifference(mSamples[i].timeStamp, mSamples[i - 1].timeStamp);
            if (timeDiff != 0) {
                const float difference(std::abs(mSamples[i].value) - std::abs(mSamples[i - 1].value));
                totalRateOfChange += std::abs(difference) / timeDiff;
            }
        }
        mAvgRateOfChange = totalRateOfChange / (mSamples.size() - 1);

        return *this;
    }

    bool checkSampleViability(T value, std::chrono::time_point<std::chrono::steady_clock> timeStamp) {
        if (mSamples.size() < n_samples / 2 || mSamples.size() < 2) {
            return true;
        }

        // Use average and rate of change to test if the new sample is viable
        const float minValue = mAvg - mMaxRateOfChange * mAvgRateOfChange;
        const float maxValue = mAvg + mMaxRateOfChange * mAvgRateOfChange;

        const auto lastSample = mSamples.back();
        const auto time_diff = calculateTimeDifference(timeStamp, lastSample.timeStamp);
        const auto newRateOfChange = static_cast<float>(value - lastSample.value) / time_diff;
        if (newRateOfChange > mMaxRateOfChange * mAvgRateOfChange) {
            return true;
        }

        return value > minValue && value < maxValue;
    }

    void copyFrom(const SampleContainer& other) {
        mAvg = other.mAvg;
        mAvgRateOfChange = other.mAvgRateOfChange;
        mSamples = other.mSamples;
        mStdDerivation = other.mStdDerivation;
        mVariance = other.mVariance;
        mMaxRateOfChange = other.mMaxRateOfChange;
    }

    void moveFrom(SampleContainer&& other) noexcept {
        mSamples = std::move(other.mSamples);
        mAvg = other.mAvg;
        mAvgRateOfChange = other.mAvgRateOfChange;
        mStdDerivation = other.mStdDerivation;
        mVariance = other.mVariance;
        mMaxRateOfChange = other.mMaxRateOfChange;
    }

    mutable std::shared_mutex mResourceMutex;
    AvgType mAvg;
    float mVariance;
    float mStdDerivation;
    float mAvgRateOfChange;
    float mMaxRateOfChange;
    RingBuffer<Sample<T>, n_samples> mSamples;
};
