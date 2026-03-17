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

template<typename T>
concept StandardLayoutType = std::is_standard_layout_v<T>;

template<typename T>
concept ValidSettingType = std::is_standard_layout_v<T>;

template<ValidSettingType SettingType, ConstexprPath Path, typename FilesystemType, auto InitType = SettingInitType::lazy_load>
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
        bool copyFilenameToBuffer(ArrayType &dst, const char *extension = "") {
            auto result = snprintf(dst->data(), dst->size(), "%.*s/%.*s%s", sizeof(FilesystemType::path.value),
                                   FilesystemType::path.value,
                                   sizeof(Path.value), Path.value, extension);
            return result > 0 && result < dst->size();
        }

        FILE *openTmpFile(bool createIfNotExists = true) {
            auto filename = SmallerBufferPoolType::get_free_buffer();
            copyFilenameToBuffer(filename, ".tmp");

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

            if (!ensure_path_exists(Path.value)) {
                Logger::log(LogLevel::Error, "Entrypoint is not there ... ");
                return ESP_FAIL;
            }

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

            Logger::log(LogLevel::Debug, "Trying to open file : %s", filename->data());
            auto opened_file = std::fopen(filename->data(), "r+");
            DoFinally closeOp( [&opened_file]() {
                std::fclose(opened_file);
            });

            if (!opened_file) {
                Logger::log(LogLevel::Warning, "File doesn't exist, trying to open tmp file: %s", filename->data());
                opened_file = openTmpFile(false);

                if (opened_file == nullptr) {
                    Logger::log(LogLevel::Warning, "Couldn't open tmp file for: %s", filename->data());
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

            auto target_file = openTmpFile(true);

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
                copyFilenameToBuffer(tmp_filename, ".tmp");
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