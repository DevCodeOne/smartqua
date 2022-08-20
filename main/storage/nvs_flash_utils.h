#pragma once

#include <shared_mutex>

#include "nvs.h"
#include "nvs_flash.h"

#include "utils/logger.h"
#include "smartqua_config.h"

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
        Logger::log(LogLevel::Info, "Initialized nvs");
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

