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
#include "utils/filesystem_utils.h"
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
        static inline constexpr char folder_name [] = "binary_data";

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
        template<typename ArrayType>
        bool copy_tmp_filename_to_buffer(ArrayType &dst) {
            auto result = snprintf(dst.data(), dst.size(), "%s/%s/%s.bin.tmp", sd_filesystem::mount_point, folder_name, SettingType::name);
            return result > 0 && result < dst.size();
        }

        template<typename ArrayType>
        bool copy_filename_to_buffer(ArrayType &dst) {
            auto result = snprintf(dst.data(), dst.size(), "%s/%s/%s.bin", sd_filesystem::mount_point, folder_name, SettingType::name);
            return result > 0 && result < dst.size();
        }

        FILE *open_tmp_file() {
            std::array<char, 64> filename{'\0'};
            copy_tmp_filename_to_buffer(filename);

            auto opened_file = std::fopen(filename.data(), "r+");

            if (!opened_file) {
                // File doesn't exist yet or can't be opened try to open the file again, and create it if it doesn't exist
                opened_file = std::fopen(filename.data(), "w+");
            }

            return opened_file;
        }

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

            std::array<char, 32> out_path{'\0'};
            auto result = snprintf(out_path.data(), out_path.size(), "%s/%s", sd_filesystem::mount_point, folder_name);

            if (result < 0) {
                ESP_LOGI("sd_card_setting", "Couldn't write folder path");
                return ESP_FAIL;
            }

            bool out_folder_exists = ensure_path_exists(out_path.data());

            if (!out_folder_exists) {
                ESP_LOGI("sd_card_setting", "Couldn't create folder structure");
                return ESP_FAIL;
            }

            m_target_file = open_tmp_file();

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
            std::array<char, 64> filename{'\0'};
            copy_filename_to_buffer(filename);
            auto opened_file = std::fopen(filename.data(), "r+");

            if (!opened_file) {
                ESP_LOGI("sd_card_setting", "There is no file to read from");
                std::fclose(opened_file);
                return ESP_FAIL;
            }

            std::fseek(opened_file, 0, SEEK_END);
            auto file_size = std::ftell(opened_file);

            if (file_size != sizeof(setting_type)) {
                ESP_LOGI("sd_card_setting", "File size of %s is %d and that isn't the correct size %d", 
                    SettingType::name,
                    static_cast<int>(file_size),
                    static_cast<int>(sizeof(setting_type)));
                std::fclose(opened_file);
                return ESP_FAIL;
            }

            fseek(opened_file, 0, SEEK_SET);
            auto read_size = fread(reinterpret_cast<void *>(&m_setting), sizeof(setting_type), 1, opened_file);

            ESP_LOGI("sd_card_setting", "Read %d bytes from the sd card", read_size * sizeof(setting_type));
            std::fclose(opened_file);

            return read_size == 1;
        }

        esp_err_t store_to_sd_card() {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            ESP_LOGI("sd_card_setting", "Writing to sd card");
            std::rewind(m_target_file);

            auto written_size = std::fwrite(reinterpret_cast<void *>(&m_setting), sizeof(setting_type), 1, m_target_file);
            std::fclose(m_target_file);
            ESP_LOGI("sd_card_setting", "Wrote %d bytes to the sd card", written_size * sizeof(setting_type));

            int rename_result = -1;
            if (written_size == 1) {
                std::array<char, 64> tmp_filename{'\0'};
                std::array<char, 64> filename{'\0'};
                copy_tmp_filename_to_buffer(tmp_filename);
                copy_filename_to_buffer(filename);
                std::remove(filename.data());
                ESP_LOGI("sd_card_setting", "Renaming %s to %s", tmp_filename.data(), filename.data());
                rename_result = std::rename(tmp_filename.data(), filename.data());
            } else {
                ESP_LOGI("sd_card_setting", "Setting couldn't be written skipping renaming to real file to avoid issues");
            }

            if (rename_result < 0) {
                ESP_LOGI("sd_card_setting", "Couldn't rename file");
            }

            m_target_file = open_tmp_file();

            
            return written_size == 1 && rename_result >= 0;
        }
    
        bool m_initialized = false;
        std::optional<sd_filesystem> m_filesystem = std::nullopt;
        FILE *m_target_file = nullptr;
        setting_type m_setting;
};