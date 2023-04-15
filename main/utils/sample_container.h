#pragma once

#include <shared_mutex>
#include <type_traits>

#include "ring_buffer.h"

template <typename T, typename AvgType = T, uint32_t n_samples = 10u>
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

    sample_container &put_sample(T value) {
        std::unique_lock _instance_lock{m_resource_mutex};

        m_samples.append(value);

        return recalculate_avg(_instance_lock);
    }

    AvgType average() const { 
        std::shared_lock _instance_lock{m_resource_mutex};

        return m_avg; 
    }

    AvgType last() const {
        return m_samples.back();
    }

   private:
    // TODO: maybe don't recalculate complete new average
    template <typename MutexType>
    sample_container &recalculate_avg(std::unique_lock<MutexType> &lock) {
        if (!lock.owns_lock()) {
            return *this;
        }

        if (m_samples.size() == 0) {
            return *this;
        }

        using SumType = std::conditional_t<std::is_floating_point_v<AvgType> 
            || std::is_floating_point_v<T>, float, int32_t>;
        SumType summed_up_values = static_cast<SumType>(m_samples.front());

        for (typename decltype(m_samples)::size_type i = 1;
             i < m_samples.size(); ++i) {
            summed_up_values += static_cast<SumType>(m_samples[i]);
        }

        m_avg = static_cast<AvgType>(summed_up_values) /
                static_cast<AvgType>(m_samples.size());

        return *this;
    }

    mutable std::shared_mutex m_resource_mutex;
    AvgType m_avg;
    ring_buffer<T, n_samples> m_samples;
};
