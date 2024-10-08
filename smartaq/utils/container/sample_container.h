#pragma once

#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <optional>
#include <cmath>
#include <ranges>

#include "ring_buffer.h"

template <typename T, typename AvgType = T, uint32_t n_samples = 10u>
requires(n_samples >= 10)
class sample_container final {
   public:
    sample_container() = default;
    sample_container(const sample_container &other) {
        std::unique_lock other_instance(other.m_resource_mutex);

        m_avg = other.m_avg;
        m_samples = other.m_samples;
    }

    sample_container(sample_container &&other) {
        std::lock(m_resource_mutex, other.m_resource_mutex);
        std::lock_guard own_mutex(m_resource_mutex, std::adopt_lock);
        std::lock_guard other_mutex(other.m_resource_mutex, std::adopt_lock);

        m_avg = std::move(other.m_avg);
        m_samples = std::move(other.m_samples);
    }

    ~sample_container() = default;

    sample_container &operator=(sample_container other) {
        std::lock(m_resource_mutex, other.m_resource_mutex);
        std::lock_guard own_mutex(m_resource_mutex, std::adopt_lock);
        std::lock_guard other_mutex(other.m_resource_mutex, std::adopt_lock);

        using std::swap;
        swap(m_avg, other.m_avg);
        swap(m_samples, other.m_samples);

        return *this;
    }

    // Return if value was accepted
    bool put_sample(T value) {
        std::unique_lock _instance_lock{m_resource_mutex};

        m_samples.append(value);

        recalculateInternalValues(_instance_lock);

        return true;
    }

    AvgType average() const {
        std::shared_lock _instance_lock{m_resource_mutex};

        return m_avg;
    }

    AvgType last() const {
        std::shared_lock _instance_lock{m_resource_mutex};

        return m_samples.back();
    }

    auto std_variance() const {
        std::shared_lock _instance_lock{m_resource_mutex};

        return m_std_variance;
    }

    auto variance() const {
        std::shared_lock _instance_lock{m_resource_mutex};

        return m_variance;
    }

    auto size() const {
        std::shared_lock _instance_lock{m_resource_mutex};

        return m_samples.size();
    }

   private:
    // TODO: maybe don't recalculate complete new average
    template <typename MutexType>
    sample_container &recalculateInternalValues(std::unique_lock<MutexType> &lock) {
        if (!lock.owns_lock()) {
            return *this;
        }

        if (m_samples.size() == 0) {
            return *this;
        }

        const auto divisor = 1.0f / m_samples.size();
        float summed_up_values = 0;

        for (typename decltype(m_samples)::size_type i = 0; i < m_samples.size(); ++i) {
            summed_up_values += m_samples[i] * divisor;
        }

        if constexpr (std::is_integral_v<AvgType>) {
            m_avg = static_cast<AvgType>(std::lround(summed_up_values));
        } else {
            m_avg = summed_up_values;
        }

        float ss = 0;
        for (typename decltype(m_samples)::size_type i = 0; i < m_samples.size(); ++i) {
            ss += (m_samples[i] - m_avg) * (m_samples[i] - m_avg);
        }

        if (m_samples.size() > 1) {
            const auto sampleDivisor = m_samples.size() - 1;
            m_variance = ss / sampleDivisor;
            m_std_variance = std::sqrt(m_variance);
        }

        return *this;
    }

    mutable std::shared_mutex m_resource_mutex;
    AvgType m_avg;
    float m_variance;
    float m_std_variance;
    ring_buffer<T, n_samples> m_samples;
};
