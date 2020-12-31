#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

class nvs_flash {
   public:
    inline nvs_flash() {
        std::unique_lock instance_guard{_instance_lock};

        if (_initialized) {
            return;
        }

        esp_err_t err = nvs_flash_init();

        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }

        _initialized = err == ESP_OK;
        ESP_LOGI("nvs_flash", "Initialized nvs");
    }

    ~nvs_flash() = default;

    operator bool() const {
        std::shared_lock instance_guard{_instance_lock};
        return _initialized;
    }

   private:
    static inline std::shared_mutex _instance_lock{};
    static inline bool _initialized{false};
};

enum struct nvs_init { instant, lazy_load };

template <typename SettingType, nvs_init InitType = nvs_init::lazy_load>
requires(std::is_standard_layout_v<SettingType>) class nvs_setting {
   public:
    using setting_type = SettingType;
    using setting_index_type = typename setting_type::index_type;

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

    template <setting_index_type index, typename T>
    nvs_setting &set_value(T new_value) {
        init_nvs();

        {
            std::unique_lock instance_guard{m_instance_mutex};
            m_setting.template set_value<index>(new_value);
        }

        if (m_initialized) {
            store_to_nvs();
        }
        return *this;
    }

    template <setting_index_type index>
    const auto &get_value() {
        init_nvs();

        {
            std::shared_lock instance_guard{m_instance_mutex};
            return m_setting.template get_value<index>();
        }
    }

   private:
    esp_err_t init_nvs() {
        {
            std::shared_lock instance_guard{m_instance_mutex};

            if (m_initialized) {
                return ESP_OK;
            }
        }

        {
            std::unique_lock instance_guard{m_instance_mutex};

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
        }

        return load_from_nvs();
    }

    esp_err_t store_to_nvs() {
        ESP_LOGI("Setting", "Writing to nvs");
        std::unique_lock instance_guard{m_instance_mutex};

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
        std::unique_lock instance_guard{m_instance_mutex};

        size_t struct_size = sizeof(SettingType);
        esp_err_t err =
            nvs_get_blob(m_nvs_handle, SettingType::name,
                         reinterpret_cast<void *>(&m_setting), &struct_size);
        if (err != ESP_OK) {
            return err;
        }

        return err;
    }

    std::shared_mutex m_instance_mutex;
    bool m_initialized = false;
    setting_type m_setting;
    std::optional<nvs_flash> m_flash;
    nvs_handle_t m_nvs_handle;
};
