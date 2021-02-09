#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <functional>

#include "esp_log.h"
#include "hx711.h"
#include "sample_container.h"
#include "thread_utils.h"

enum struct loadcell_status { uninitialized, success, failure };

namespace Detail {
template <uint8_t n_samples = 10u, uint32_t interval_millis = 1000>
class loadcell_resource final {
   public:
    loadcell_resource(uint8_t sck, uint8_t dout) : m_dev{.dout = static_cast<gpio_num_t>(dout),
                     .pd_sck = static_cast<gpio_num_t>(sck),
                     .gain = HX711_GAIN_A_64}{
        create_sensor_sampling_thread();
    }
    loadcell_resource(loadcell_resource &&other) = delete;
    ~loadcell_resource() = default;

    void create_sensor_sampling_thread() {
        thread_creator::create_thread(loadcell_resource::read_samples, "scale_read_thread", reinterpret_cast<void *>(this));
    }

    loadcell_status read_value(int32_t *value) {
        *value = m_values.average();
        return _status;
    }

    // TODO: add some sort of stop to this
    static void read_samples(void *ptr) {
        // TODO: Make number of retries configurable
        int32_t value = 0;
        auto thiz = reinterpret_cast<loadcell_resource<n_samples, interval_millis> *>(ptr);
        for (unsigned int i = 0; i < 10; ++i) {
            auto status = hx711_init(&thiz->m_dev);
        
            if (status != ESP_OK) {
                ESP_LOGE(__PRETTY_FUNCTION__, "Couldn't initialize scale");
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        while (1) {
            esp_err_t wait_result = hx711_wait(&thiz->m_dev, interval_millis);
            bool is_ready = false;

            if (wait_result != ESP_OK || hx711_is_ready(&thiz->m_dev, &is_ready) != ESP_OK ||
                !is_ready) {
                ESP_LOGE(__PRETTY_FUNCTION__, "Scale wasn't ready");
                std::this_thread::yield();
            }

            esp_err_t read_result = hx711_read_data(&thiz->m_dev, &value);

            if (read_result != ESP_OK) {
                ESP_LOGE(__PRETTY_FUNCTION__, "Failed to read from scale");
                std::this_thread::yield();
            }

            // ESP_LOGI(__PRETTY_FUNCTION__, "Read raw value %d", value);
            thiz->m_values.put_sample(value);

        }
    }

    friend class loadcell;

    // sample_container is thread-safe
    sample_container<int32_t, float, n_samples> m_values{};
    hx711_t m_dev;
    static inline std::once_flag _thread_flag{};
    static inline loadcell_status _status = loadcell_status::uninitialized;

};
}  // namespace Detail

class loadcell {
   public:
    loadcell(uint8_t sck, uint8_t dout, int32_t offset = 0);

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
    Detail::loadcell_resource<20> m_loadcell_resource;
};
