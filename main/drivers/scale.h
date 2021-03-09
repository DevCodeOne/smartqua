#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <functional>

#include "esp_log.h"
#include "hx711.h"
#include "utils/sample_container.h"
#include "utils/task_pool.h"
#include "drivers/device_types.h"

enum struct loadcell_status { uninitialized, success, failure };

struct loadcell_config {
    int32_t offset;
    float scale;
    uint8_t sck;
    uint8_t dout;
};

// TODO: use the task_pool api
namespace Detail {
template <uint8_t n_samples = 10u, uint32_t interval_millis = 1000>
class loadcell_resource final {
   public:
    loadcell_resource(loadcell_config *conf) : m_dev{
                    .dout = static_cast<gpio_num_t>(conf->dout),
                    .pd_sck = static_cast<gpio_num_t>(conf->sck),
                    .gain = HX711_GAIN_A_64}{
        create_sensor_sampling_thread();
    }
    ~loadcell_resource() = default;

    void create_sensor_sampling_thread() {
        task_pool<max_task_pool_size>::post_task(single_task{});
    }

    loadcell_status read_value(int32_t *value) {
        // *value = m_values.average();
        return loadcell_status::failure;
    }

    // TODO: add some sort of stop to this
    static void read_samples(void *) {
        // TODO: Make number of retries configurable
        /*
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

        }*/
        while (1) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    friend class loadcell;

    // sample_container is thread-safe
    sample_container<int32_t, float, n_samples> m_values{};
    hx711_t m_dev;
};
}  // namespace Detail

class loadcell {
   public:
    static inline constexpr char name[] = "hx711_driver";

    static std::optional<loadcell> create_driver(const std::string_view input, device_config &device_conf_out);
    static std::optional<loadcell> create_driver(const device_config *config);

    device_operation_result write_value(const device_values &value);
    device_operation_result read_value(device_values &out) const;
    device_operation_result write_device_options(const char *json_input, size_t input_len);
    device_operation_result get_info(char *output, size_t output_buffer_len) const;

   private:
    loadcell(const device_config *config);

    const device_config *m_conf = nullptr;
    Detail::loadcell_resource<20> m_loadcell_resource;
};
