#pragma once

#include <array>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <charconv>
#include <cstdio>

#include "esp_http_client.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_flash_utils.h"

#include "utils/sd_filesystem.h"
#include "utils/filesystem_utils.h"
#include "utils/utils.h"
#include "utils/logger.h"
#include "utils/stack_string.h"
#include "storage/rest_storage.h"
#include "smartqua_config.h"

enum struct SettingInitType { instant, lazy_load };

#if TARGET_DEVICE == ESP32
template<typename SettingType, auto InitType = SettingInitType::lazy_load>
class NvsSetting {
   public:
    static_assert(std::is_standard_layout_v<SettingType>,
        "SettingType has to conform to the standard layout concept");

    using setting_type = SettingType;

    NvsSetting() { initialize(); }

    ~NvsSetting() {
        if (m_initialized) {
            nvs_close(m_nvs_handle);
        }
    }

    void initialize() {
        if (InitType == SettingInitType::instant) {
            init_nvs();
        }
    }

    template <typename T>
    NvsSetting &set_value(T new_value) {
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
            Logger::log(LogLevel::Error, "Nvs flash isn't initialized");
            return ESP_FAIL;
        }

        auto err =
            nvs_open(SettingType::name, NVS_READWRITE, &m_nvs_handle);

        m_initialized = err == ESP_OK;
        Logger::log(LogLevel::Warning, "Couldn't initialize nvs setting");
        if (err != ESP_OK) {
            return err;
        }

        return load_from_nvs();
    }

    esp_err_t store_to_nvs() {
        Logger::log(LogLevel::Info, "Writing to nvs");

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
        Logger::log(LogLevel::Info, "Loading from nvs");

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
#endif

template<typename SettingType, auto InitType = SettingInitType::lazy_load>
class FilesystemSetting {
    public:
        static inline constexpr char folder_name [] = "binary_data";

        static_assert(std::is_standard_layout_v<SettingType>, 
            "SettingType has to conform to the standard layout concept");

        using setting_type = SettingType;

        FilesystemSetting() { initialize(); }

        ~FilesystemSetting() = default;

        void initialize() {
            if (InitType == SettingInitType::instant) {
                init_sd_card();
            }
        }

        template<typename T>
        FilesystemSetting &set_value(T new_value) {
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
            stack_string<64> filename;
            copy_tmp_filename_to_buffer(filename);

            auto opened_file = std::fopen(filename.data(), "r+");

            if (opened_file == nullptr) {
                // File doesn't exist yet or can't be opened try to open the file again, and create it if it doesn't exist
                opened_file = std::fopen(filename.data(), "w+");
            }

            return opened_file;
        }

        esp_err_t init_sd_card() {
            if (m_initialized) {
                return ESP_OK;
            }

            Logger::log(LogLevel::Info, "Initializing sd card");
            if (!m_filesystem) {
                m_filesystem = sd_filesystem{};
            }

            if (!m_filesystem.value()) {
                Logger::log(LogLevel::Error, "Filesystem is not valid");
                return ESP_FAIL;
            }

            std::array<char, name_length> out_path{'\0'};
            auto result = snprintf(out_path.data(), out_path.size(), "%s/%s", sd_filesystem::mount_point, folder_name);

            if (result < 0) {
                Logger::log(LogLevel::Error, "Couldn't write folder path");
                return ESP_FAIL;
            }

            bool out_folder_exists = ensure_path_exists(out_path.data());

            if (!out_folder_exists) {
                Logger::log(LogLevel::Error, "Couldn't create folder structure");
                return ESP_FAIL;
            }

            auto target_file = open_tmp_file();
            DoFinally closeOp( [&target_file]() {
                std::fclose(target_file);
            });

            if (!target_file) {
                Logger::log(LogLevel::Error, "Couldn't open target file");
                return ESP_FAIL;
            }

            m_initialized = true;

            return load_from_sd_card();
        }

        esp_err_t load_from_sd_card() {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            Logger::log(LogLevel::Info, "Loading from sd card");
            std::array<char, 64> filename{'\0'};
            copy_filename_to_buffer(filename);
            auto opened_file = std::fopen(filename.data(), "r+");
            DoFinally closeOp( [&opened_file]() {
                std::fclose(opened_file);
            });

            if (!opened_file) {
                Logger::log(LogLevel::Warning, "There is no file to read from");
                return ESP_FAIL;
            }

            std::fseek(opened_file, 0, SEEK_END);
            auto file_size = std::ftell(opened_file);

            if (file_size != sizeof(setting_type)) {
                Logger::log(LogLevel::Warning, "File size of %s is %d and that isn't the correct size %d", 
                    SettingType::name,
                    static_cast<int>(file_size),
                    static_cast<int>(sizeof(setting_type)));
                return ESP_FAIL;
            }

            std::fseek(opened_file, 0, SEEK_SET);
            auto read_size = fread(reinterpret_cast<void *>(&m_setting), sizeof(setting_type), 1, opened_file);

            Logger::log(LogLevel::Info, "Read %d bytes from the sd card", read_size * sizeof(setting_type));

            return read_size == 1;
        }

        esp_err_t store_to_sd_card() {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            Logger::log(LogLevel::Info, "Writing to sd card");

            auto target_file = open_tmp_file();
            rewind(target_file);

            auto written_size = fwrite(reinterpret_cast<void *>(&m_setting), sizeof(setting_type), 1, target_file);
            fclose(target_file);
            Logger::log(LogLevel::Info, "Wrote %d bytes to the sd card", written_size * sizeof(setting_type));

            int rename_result = -1;
            if (written_size == 1) {
                stack_string<64> tmp_filename;
                stack_string<64> filename;
                // TODO: check results of both methods
                copy_tmp_filename_to_buffer(tmp_filename);
                copy_filename_to_buffer(filename);
                std::remove(filename.data());
                Logger::log(LogLevel::Info, "Renaming %s to %s", tmp_filename.data(), filename.data());
                rename_result = std::rename(tmp_filename.data(), filename.data());
            } else {
                Logger::log(LogLevel::Warning, "Setting couldn't be written skipping renaming to real file to avoid issues");
            }

            if (rename_result < 0) {
                Logger::log(LogLevel::Error, "Couldn't rename file");
            }

            return written_size == 1 && rename_result >= 0;
        }
    
        bool m_initialized = false;
        std::optional<sd_filesystem> m_filesystem = std::nullopt;
        setting_type m_setting;
};

template<typename SettingType, SettingInitType InitType = SettingInitType::lazy_load>
class RestRemoteSetting {
    public:
        static_assert(std::is_standard_layout_v<SettingType>,
            "SettingType has to conform to the standard layout concept");

        RestRemoteSetting() {
            if (InitType == SettingInitType::instant) {
                retrieveInitialValue();
            }
        }

        ~RestRemoteSetting() = default;

        template<typename T>
        RestRemoteSetting &set_value(const T &new_value) {
            mValue = new_value;
            restStorage.writeData(reinterpret_cast<const char *>(&mValue), sizeof(mValue));

            return *this;
        }

        const auto &get_value() {
            retrieveInitialValue();

            return mValue;
        }

        operator bool() const {
            return isValid();
        }

        bool isValid() const {
            return true;
        }

        template<size_t ArraySize>
        static bool generateRestTarget(std::array<char, ArraySize> &dst) {
            std::array<uint8_t, 6> mac;
            esp_efuse_mac_get_default(mac.data());
            snprintf(dst.data(), dst.size(), "http://%s/values/%02x-%02x-%02x-%02x-%02x-%02x-%s", 
                remote_setting_host,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                SettingType::name);

            // TODO: check return value
            return true;
        }

    private:

        void retrieveInitialValue() {
            restStorage.retrieveData(reinterpret_cast<char *>(&mValue), sizeof(mValue));
        }

        RestStorage<RestRemoteSetting, RestDataType::Binary> restStorage;
        SettingType mValue{};
};