#pragma once

#include <array>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <cstdio>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_flash_utils.h"

#include "utils/sd_filesystem.h"
#include "smartqua_config.h"

enum struct setting_init { instant, lazy_load };

using nvs_init = setting_init;

template<typename SettingType, nvs_init InitType = nvs_init::lazy_load>
class nvs_setting {
   public:
    static_assert(std::is_standard_layout_v<SettingType>,
        "SettingType has to conform to the standard layout concept");

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

template<typename SettingType, setting_init InitType = setting_init::lazy_load>
class sd_card_setting {
    public:
        static_assert(std::is_standard_layout_v<SettingType>, 
            "SettingType has to conform to the standard layout concept");

        using setting_type = SettingType;

        sd_card_setting() { initialize(); }

        ~sd_card_setting() {
            if (m_initialized) {
                std::fclose(m_target_file);
            }
        }

        void initialize() {
            if (InitType == setting_init::instant) {
                init_sd_card();
            }
        }

        template<typename T>
        sd_card_setting &set_value(T new_value) {
            init_sd_card();

            m_setting = new_value;

            if (m_initialized) {
                store_to_sd_card();
            }

            return *this;
        }

        const auto &get_value() {
            init_sd_card();

            return m_setting;
        }

    private:
        esp_err_t init_sd_card() {
            ESP_LOGI("sd_card_setting", "Initializing sd card");

            if (m_initialized) {
                return ESP_OK;
            }

            if (!m_filesystem) {
                m_filesystem = sd_filesystem{};
            }

            if (!m_filesystem.value()) {
                ESP_LOGI("sd_card_setting", "Filesystem is not valid");
                return ESP_FAIL;
            }

            std::array<char, name_length * 2> filename{'\0'};
            snprintf(filename.data(), filename.size() - 1, "%s/%s.bin", sd_filesystem::mount_point, SettingType::name);
            m_target_file = std::fopen(filename.data(), "r+");

            if (!m_target_file) {
                // File doesn't exist yet or can't be opened try to open the file again, and create it if it doesn't exist
                m_target_file = std::fopen(filename.data(), "w+");
            }

            if (!m_target_file) {
                ESP_LOGI("sd_card_setting", "Couldn't open target file");
                return ESP_FAIL;
            }

            m_initialized = true;

            return load_from_sd_card();
        }

        esp_err_t load_from_sd_card() {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            ESP_LOGI("sd_card_setting", "Loading from sd card");
            fseek(m_target_file, 0, SEEK_END);
            auto file_size = std::ftell(m_target_file);

            if (file_size != sizeof(setting_type)) {
                ESP_LOGI("sd_card_setting", "File size is %d and that isn't the correct size", static_cast<int>(file_size));
                return ESP_FAIL;
            }

            fseek(m_target_file, 0, SEEK_SET);
            auto read_size = fread(reinterpret_cast<void *>(&m_setting), sizeof(setting_type), 1, m_target_file);

            ESP_LOGI("sd_card_setting", "Read %d bytes from the sd card", read_size * sizeof(setting_type));

            return read_size == 1;
        }

        esp_err_t store_to_sd_card() {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            ESP_LOGI("sd_card_setting", "Writing to sd card");
            rewind(m_target_file);

            auto written_size = fwrite(reinterpret_cast<void *>(&m_setting), sizeof(setting_type), 1, m_target_file);
            fflush(m_target_file);
            fsync(fileno(m_target_file));

            ESP_LOGI("sd_card_setting", "Wrote %d bytes to the sd card", written_size * sizeof(setting_type));
            
            return written_size == 1;
        }
    
        bool m_initialized = false;
        std::optional<sd_filesystem> m_filesystem = std::nullopt;
        FILE *m_target_file = nullptr;
        setting_type m_setting;
};