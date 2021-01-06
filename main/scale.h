#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>

#include "esp_log.h"
#include "hx711.h"
#include "sample_container.h"
#include "thread_creator.h"

template <uint8_t sck, uint8_t dout>
class loadcell;

enum struct loadcell_status { uninitialized, success, failure };

namespace Detail {
template <uint8_t sck, uint8_t dout, uint8_t n_samples = 10u, uint32_t interval_millis = 1000>
class loadcell_resource final {
   public:
    loadcell_resource() = default;
    ~loadcell_resource() = default;

    void initialize() {
        std::call_once(_thread_flag, loadcell_resource::create_sensor_sampling_thread);
    }

    static void create_sensor_sampling_thread() {
        thread_creator::create_thread(loadcell_resource::read_samples, "scale_read_thread");
    }

    loadcell_status read_value(int32_t *value) {
        *value = _values.average();
        return _status;
    }

    // TODO: add some sort of stop to this
    static void read_samples(void *unused) {
        auto status = hx711_init(&_dev);
        int32_t value = 0;
        
        if (status != ESP_OK) {
            ESP_LOGE(__PRETTY_FUNCTION__, "Couldn't initialize scale");
            return;
        }
        while (1) {
            esp_err_t wait_result = hx711_wait(&_dev, interval_millis);
            bool is_ready = false;

            if (wait_result != ESP_OK || hx711_is_ready(&_dev, &is_ready) != ESP_OK ||
                !is_ready) {
                ESP_LOGE(__PRETTY_FUNCTION__, "Scale wasn't ready");
                std::this_thread::yield();
            }

            esp_err_t read_result = hx711_read_data(&_dev, &value);

            if (read_result != ESP_OK) {
                ESP_LOGE(__PRETTY_FUNCTION__, "Failed to read from scale");
                std::this_thread::yield();
            }

            // ESP_LOGI(__PRETTY_FUNCTION__, "Read raw value %d", value);
            _values.put_sample(value);

        }
    }

    friend class loadcell<sck, dout>;

    // sample_container is thread-safe
    static inline sample_container<int32_t, float, n_samples> _values{};
    static inline std::once_flag _thread_flag{};
    static inline loadcell_status _status = loadcell_status::uninitialized;

    static inline hx711_t _dev = {.dout = static_cast<gpio_num_t>(dout),
                     .pd_sck = static_cast<gpio_num_t>(sck),
                     .gain = HX711_GAIN_A_64};
};
}  // namespace Detail

template <uint8_t sck, uint8_t dout>
class loadcell {
   public:
    loadcell(int32_t offset = 0);

    loadcell_status init_scale();
    loadcell_status read_raw(int32_t *value);
    loadcell_status read_value(float *value);
    loadcell_status tare();
    loadcell_status set_offset(int32_t value);
    loadcell_status set_scale(float scale);

    int32_t get_offset() const {
        std::lock_guard instance_lock{m_resource_lock};
        return m_offset;
    }

    float get_scale() const {
        std::lock_guard instance_lock{m_resource_lock};
        return m_scale;
    }

   private:
    int32_t m_offset = 0;
    float m_scale = 1.0f;
    mutable std::shared_mutex m_resource_lock;
    Detail::loadcell_resource<sck, dout, 20> m_loadcell_resource;
};

template <uint8_t sck, uint8_t dout>
loadcell<sck, dout>::loadcell(int32_t offset) : m_offset(offset) {}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::init_scale() {
    std::unique_lock instance_lock{m_resource_lock};

    m_loadcell_resource.initialize();
    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::read_raw(int32_t *value) {
    std::shared_lock instance_lock{m_resource_lock};

    m_loadcell_resource.read_value(value);
    *value -= m_offset;

    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::tare() {
    int32_t value = 0;
    loadcell_status result = loadcell_status::failure;
    {
        std::shared_lock instance_lock{m_resource_lock};

        // TODO: maybe use even more samples ?
        result = read_raw(&value);

        if (result != loadcell_status::success) {
            return result;
        }
    }

    // Yes race condition ahead, but no lock up, for now that's good enough
    int32_t old_offset = get_offset();
    set_offset(old_offset + value);

    return result;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::set_offset(int32_t value) {
    std::unique_lock instance_lock{m_resource_lock};
    m_offset = value;

    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::set_scale(float scale) {
    std::unique_lock instance_lock{m_resource_lock};
    if (std::fabs(scale) < 0.05f) {
        return loadcell_status::failure;
    }

    m_scale = scale;
    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::read_value(float *value) {
    std::shared_lock instance_lock{m_resource_lock};

    int32_t raw_value = 0;
    auto result = read_raw(&raw_value);

    if (result != loadcell_status::success) {
        return result;
    }

    *value = static_cast<float>(raw_value) / m_scale;

    return loadcell_status::success;
}