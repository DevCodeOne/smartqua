#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <utility>

#include "esp_log.h"
#include "hx711.h"

enum struct loadcell_status { uninitialized, success, failure };

// TODO: maybe add seperate thread to read the values into a queue to read from
template <uint8_t sck, uint8_t dout>
class loadcell {
   public:
    loadcell(int32_t offset = 0);

    loadcell_status init_scale();
    loadcell_status read_value(int32_t *value);
    loadcell_status read_scale(float *value);
    std::pair<uint8_t, loadcell_status> read_average(uint8_t times,
                                                     int32_t *value);
    std::pair<uint8_t, loadcell_status> read_in_units(uint8_t times,
                                                      float *value);
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
    hx711_t m_dev = {.dout = static_cast<gpio_num_t>(dout),
                     .pd_sck = static_cast<gpio_num_t>(sck),
                     .gain = HX711_GAIN_A_64};
    loadcell_status m_status = loadcell_status::uninitialized;
    int32_t m_offset = 0;
    float m_scale = 1.0f;
    mutable std::recursive_mutex m_resource_lock;
};

template <uint8_t sck, uint8_t dout>
loadcell<sck, dout>::loadcell(int32_t offset) : m_offset(offset) {}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::init_scale() {
    std::lock_guard instance_lock{m_resource_lock};
    m_status = hx711_init(&m_dev) == ESP_OK ? loadcell_status::success
                                            : loadcell_status::failure;

    return m_status;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::read_value(int32_t *value) {
    std::lock_guard instance_lock{m_resource_lock};

    if (m_status != loadcell_status::success) {
        return m_status;
    }

    esp_err_t wait_result = hx711_wait(&m_dev, 1000);
    bool is_ready = false;

    if (wait_result != ESP_OK || hx711_is_ready(&m_dev, &is_ready) != ESP_OK ||
        !is_ready) {
        ESP_LOGI("Scale", "Couldn't read from Scale");
        return loadcell_status::failure;
    }

    esp_err_t read_result = hx711_read_data(&m_dev, value);

    if (read_result != ESP_OK) {
        return loadcell_status::failure;
    }
    ESP_LOGI("Scale", "Read raw value %d", *value);

    *value -= m_offset;

    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
std::pair<uint8_t, loadcell_status> loadcell<sck, dout>::read_average(
    uint8_t times, int32_t *value) {
    std::lock_guard instance_lock{m_resource_lock};

    int32_t summed_values = 0;
    int32_t read_n_values = 0;
    loadcell_status last_status;

    if (m_status != loadcell_status::success) {
        return std::make_pair(0, m_status);
    }

    int32_t current_value = 0;
    for (; read_n_values < times; ++read_n_values) {
        last_status = read_value(&current_value);
        ESP_LOGI("Scale", "Read value %d", current_value);

        if (last_status != loadcell_status::success) {
            break;
        }

        summed_values += current_value;
    }

    if (read_n_values != 0) {
        *value = summed_values / read_n_values;
    }

    return std::make_pair(read_n_values, last_status);
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::tare() {
    std::lock_guard instance_lock{m_resource_lock};
    int32_t value = 0;

    auto result = read_average(20, &value);

    if (result.second != loadcell_status::success) {
        return result.second;
    }

    set_offset(get_offset() + value);

    return result.second;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::set_offset(int32_t value) {
    std::lock_guard instance_lock{m_resource_lock};
    m_offset = value;

    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::set_scale(float scale) {
    std::lock_guard instance_lock{m_resource_lock};
    if (std::fabs(scale) < 0.05f) {
        return loadcell_status::failure;
    }

    m_scale = scale;
    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
loadcell_status loadcell<sck, dout>::read_scale(float *value) {
    std::lock_guard instance_lock{m_resource_lock};
    int32_t raw_value = 0;
    auto result = read_value(&raw_value);

    if (result != loadcell_status::success) {
        return result;
    }

    *value = static_cast<float>(raw_value) / m_scale;

    return loadcell_status::success;
}

template <uint8_t sck, uint8_t dout>
std::pair<uint8_t, loadcell_status> loadcell<sck, dout>::read_in_units(
    uint8_t times, float *value) {
    std::lock_guard instance_lock{m_resource_lock};
    int32_t raw_value = 0;
    auto result = read_average(times, &raw_value);

    *value = static_cast<float>(raw_value) / m_scale;

    return result;
}

