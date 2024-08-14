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

// #include "utils/sd_filesystem.h"
#include "utils/filesystem_utils.h"
#include "utils/utils.h"
#include "utils/logger.h"
#include "utils/stack_string.h"
#include "storage/rest_storage.h"
#include "build_config.h"

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

template<typename SettingType, ConstexprPath Path, typename FilesystemType, auto InitType = SettingInitType::lazy_load>
requires (std::is_standard_layout_v<SettingType>)
class FilesystemSetting final {
    public:
        FilesystemSetting() { initialize(); }

        ~FilesystemSetting() = default;

        void initialize() {
            std::unique_lock instanceGard{instanceMutex};
            if (InitType == SettingInitType::instant) {
                initFilesystem();
            }
        }

        template<typename T>
        FilesystemSetting &set_value(T new_value) {
            std::unique_lock instanceGard{instanceMutex};
            initFilesystem();

            m_setting = new_value;

            if (m_initialized) {
                storeToFilesystem();
            }

            return *this;
        }

        const auto &get_value() {
            std::unique_lock instanceGard{instanceMutex};
            initFilesystem();

            return m_setting;
        }

    private:
        template<typename ArrayType>
        bool copyTmpFilenameToBuffer(ArrayType &dst) {
            auto result = snprintf(dst->data(), dst->size(), "%s/%s.tmp", FilesystemType::MountPointPath, Path.value);
            return result > 0 && result < dst->size();
        }

        template<typename ArrayType>
        bool copyFilenameToBuffer(ArrayType &dst) {
            auto result = snprintf(dst->data(), dst->size(), "%s/%s", FilesystemType::MountPointPath, Path.value);
            return result > 0 && result < dst->size();
        }

        FILE *openTmpFile(bool createIfNotExists = true) {
            auto filename = SmallerBufferPoolType::get_free_buffer();
            copyTmpFilenameToBuffer(filename);

            Logger::log(LogLevel::Info, "Trying to open tmp file : %s", filename->data());
            auto opened_file = fopen(filename->data(), "r+");

            if (opened_file == nullptr && createIfNotExists) {
                // File doesn't exist yet or can't be opened try to open the file again, and create it if it doesn't exist
                Logger::log(LogLevel::Info, "Couldn't open tmp file -> creating file");
                opened_file = fopen(filename->data(), "w+");

                if (!opened_file) {
                    Logger::log(LogLevel::Info, "Couldn't create tmp file");
                }
            }

            return opened_file;
        }

        esp_err_t initFilesystem() {
            if (m_initialized) {
                return ESP_OK;
            }

            Logger::log(LogLevel::Info, "Initializing filesystem");
            if (!m_filesystem) {
                m_filesystem = FilesystemType::create();
            }

            if (!m_filesystem.has_value()) {
                Logger::log(LogLevel::Error, "Filesystem is not valid");
                return ESP_FAIL;
            }

            /*
            std::array<char, name_length> out_path{'\0'};
            auto result = snprintf(out_path.data(), out_path.size(), "%s/%s", Path.value, FolderName);

            if (result < 0) {
                Logger::log(LogLevel::Error, "Couldn't write folder path");
                return ESP_FAIL;
            }

            bool out_folder_exists = ensure_path_exists(out_path.data());

            if (!out_folder_exists) {
                Logger::log(LogLevel::Error, "Couldn't create folder structure");
                return ESP_FAIL;
            }

            auto target_file = openTmpFile();
            DoFinally closeOp( [&target_file]() {
                std::fclose(target_file);
            });

            if (!target_file) {
                Logger::log(LogLevel::Error, "Couldn't open target file %s", SettingType::name);
                return ESP_FAIL;
            }*/

            m_initialized = true;

            return loadFromFilesystem();
        }

        esp_err_t loadFromFilesystem() {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            Logger::log(LogLevel::Info, "Loading from filesystem");
            auto filename = SmallerBufferPoolType::get_free_buffer();
            copyFilenameToBuffer(filename);
            auto opened_file = std::fopen(filename->data(), "r+");
            DoFinally closeOp( [&opened_file]() {
                std::fclose(opened_file);
            });

            if (!opened_file) {
                Logger::log(LogLevel::Warning, "There is no file to read from ... trying to read temporary file");
                opened_file = openTmpFile(false);

                if (opened_file == nullptr) {
                    Logger::log(LogLevel::Warning, "Couldn't open tmp file");
                    return ESP_FAIL;
                }
            }

            std::fseek(opened_file, 0, SEEK_END);
            auto file_size = std::ftell(opened_file);

            if (file_size != sizeof(SettingType)) {
                Logger::log(LogLevel::Warning, "File size of %s is %d and that isn't the correct size %d", 
                    SettingType::name,
                    static_cast<int>(file_size),
                    static_cast<int>(sizeof(SettingType)));
                return ESP_FAIL;
            }

            std::fseek(opened_file, 0, SEEK_SET);
            auto read_size = std::fread(reinterpret_cast<void *>(&m_setting), sizeof(SettingType), 1, opened_file);

            Logger::log(LogLevel::Info, "Read %d bytes from the sd card", read_size * sizeof(SettingType));

            return read_size == 1;
        }

        esp_err_t storeToFilesystem() {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            Logger::log(LogLevel::Info, "Writing to filesystem");

            if (std::memcmp(reinterpret_cast<void *>(&m_written), reinterpret_cast<void *>(&m_setting), sizeof(SettingType)) == 0) {
                Logger::log(LogLevel::Info, "Setting didn't change -> don't write to flash %s", Path);
                return ESP_OK;
            }

            auto target_file = openTmpFile();

            if (!target_file) {
                Logger::log(LogLevel::Info, "Couldn't open tmp file");
                return ESP_FAIL;
            }

            std::rewind(target_file);

            Logger::log(LogLevel::Info, "Opened tmp file");

            auto written_size = std::fwrite(reinterpret_cast<void *>(&m_setting), sizeof(SettingType), 1, target_file);
            std::fclose(target_file);
            Logger::log(LogLevel::Info, "Wrote %d bytes to the filesystem", written_size * sizeof(SettingType));

            int rename_result = -1;
            if (written_size == 1) {
                auto filename = SmallerBufferPoolType::get_free_buffer();
                auto tmp_filename = SmallerBufferPoolType::get_free_buffer();
                // TODO: check results of both methods
                copyTmpFilenameToBuffer(tmp_filename);
                copyFilenameToBuffer(filename);
                std::remove(filename->data());
                Logger::log(LogLevel::Info, "Renaming %s to %s", tmp_filename->data(), filename->data());
                rename_result = std::rename(tmp_filename->data(), filename->data());
            } else {
                Logger::log(LogLevel::Warning, "Setting couldn't be written skipping renaming to real file to avoid issues");
            }

            if (rename_result < 0) {
                Logger::log(LogLevel::Error, "Couldn't rename file");
            }

            if (written_size != 1 || rename_result < 0) {
                return false;
            }

            m_written = m_setting;
            return true;
        }
    
        bool m_initialized = false;
        std::optional<FilesystemType> m_filesystem = std::nullopt;
        std::mutex instanceMutex;
        SettingType m_setting;
        SettingType m_written;
};

template<typename SettingType, SettingInitType InitType = SettingInitType::lazy_load>
struct RestRemoteSettingPathGenerator {
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
};

template<typename SettingType, SettingInitType InitType = SettingInitType::lazy_load>
requires (std::is_standard_layout_v<SettingType>)
class RestRemoteSetting {
    public:
        using PathGeneratorType = RestRemoteSettingPathGenerator<SettingType, InitType>;

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

        [[nodiscard]] const auto &get_value() {
            retrieveInitialValue();

            return mValue;
        }

        [[nodiscard]] explicit operator bool() const {
            return isValid();
        }

        [[nodiscard]] bool isValid() const {
            return true;
        }

        

    private:

        void retrieveInitialValue() {
            restStorage.retrieveData(reinterpret_cast<char *>(&mValue), sizeof(mValue));
        }

        RestStorage<PathGeneratorType, RestDataType::Binary> restStorage;
        SettingType mValue{};
};