#pragma once

#include <array>
#include <optional>
#include <shared_mutex>
#include <type_traits>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_flash_utils.h"

enum struct nvs_init { instant, lazy_load };

template<typename SettingType, nvs_init InitType = nvs_init::lazy_load>
class nvs_setting {
   public:
    static_assert(std::is_standard_layout_v<SettingType>,
                  "SettingType has to be standard layout");

    using setting_type = SettingType;

    nvs_setting() { initialize(); }

    ~nvs_setting() {
        if (m_initialized) {
            nvs_close(m_nvs_handle);
        }
    }

    void initialize() {
        if (InitType == nvs_init::instant) {
            init_nvs();
        }
    }

    template <typename T>
    nvs_setting &set_value(T new_value) {
        init_nvs();

        m_setting = new_value;

        if (m_initialized) {
            store_to_nvs();
        }
        return *this;
    }

    const auto &get_value() {
        init_nvs();
        return m_setting;
    }

   private:
    esp_err_t init_nvs() {
        if (m_initialized) {
            return ESP_OK;
        }

        m_flash = nvs_flash{};

        if (!*m_flash) {
            ESP_LOGI("Setting", "Nvs flash isn't initialized");
            return ESP_FAIL;
        }

        auto err =
            nvs_open(SettingType::name, NVS_READWRITE, &m_nvs_handle);

        m_initialized = err == ESP_OK;
        ESP_LOGI("Setting", "Couldn't initialize nvs setting");
        if (err != ESP_OK) {
            return err;
        }

        return load_from_nvs();
    }

    esp_err_t store_to_nvs() {
        ESP_LOGI("Setting", "Writing to nvs");

        esp_err_t err = nvs_set_blob(m_nvs_handle, SettingType::name,
                                     reinterpret_cast<void *>(&m_setting),
                                     sizeof(SettingType));

        if (err != ESP_OK) {
            return err;
        }

        err = nvs_commit(m_nvs_handle);

        return err;
    }

    esp_err_t load_from_nvs() {
        ESP_LOGI("Setting", "Loading from nvs");

        size_t struct_size = sizeof(SettingType);
        esp_err_t err =
            nvs_get_blob(m_nvs_handle, SettingType::name,
                         reinterpret_cast<void *>(&m_setting), &struct_size);
        if (err != ESP_OK) {
            return err;
        }

        return err;
    }

    bool m_initialized = false;
    setting_type m_setting;
    std::optional<nvs_flash> m_flash;
    nvs_handle_t m_nvs_handle;
};