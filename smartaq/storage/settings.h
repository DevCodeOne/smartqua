#pragma once

#include <array>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <charconv>
#include <cstdio>

#include "utils/filesystem_utils.h"
#include "utils/utils.h"
#include "utils/type_helper.h"
#include "utils/logger.h"
#include "utils/stack_string.h"
#include "utils/serialization/serializer.h"
#include "build_config.h"

enum struct SettingInitType { instant, lazy_load };

template<typename SettingType, ConstexprPath Path, typename FilesystemType, auto InitType = SettingInitType::lazy_load>
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

            Serializer<SettingType>::serialize(m_setting, new_value, [&](const auto &name, const auto &value)
            {
                storeToFilesystem(name, value);
            });
            m_setting = new_value;

            return *this;
        }

        const auto &get_value() {
            std::unique_lock instanceGard{instanceMutex};
            initFilesystem();
            Serializer<SettingType>::deserialize(m_setting, [&](const auto &name, auto &value)
            {
                loadFromFilesystem(name, value);
            });

            return m_setting;
        }

    private:

        template<typename ArrayType>
        bool copyFilenameToBuffer(ArrayType& dst, const char* filename, const char* extension = "")
        {
            auto result = snprintf(dst->data(), dst->size(), "%.*s/%.*s/%s.%s", sizeof(FilesystemType::path.value),
                                   FilesystemType::path.value,
                                   sizeof(Path.value), Path.value, filename, extension);
            return result > 0 && result < dst->size();
        }

        FILE *openTmpFile(const char *filename, bool createIfNotExists = true) {
            auto filenameBuffer = SmallerBufferPoolType::get_free_buffer();
            copyFilenameToBuffer(filenameBuffer, filename, ".tmp");

            Logger::log(LogLevel::Info, "Trying to open tmp file : %s", filenameBuffer->data());
            auto opened_file = fopen(filenameBuffer->data(), "r+");

            if (opened_file == nullptr && createIfNotExists) {
                // File doesn't exist yet or can't be opened try to open the file again, and create it if it doesn't exist
                Logger::log(LogLevel::Info, "Couldn't open tmp file -> creating file");
                opened_file = fopen(filenameBuffer->data(), "w+");

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
            return ESP_OK;
        }

        template<IsByteCopyable T>
        esp_err_t loadFromFilesystem(const char *filename, T &value) {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            Logger::log(LogLevel::Info, "Loading from filesystem");
            auto filenameBuffer = SmallerBufferPoolType::get_free_buffer();
            copyFilenameToBuffer(filenameBuffer, filename);

            Logger::log(LogLevel::Debug, "Trying to open file : %s", filenameBuffer->data());
            auto opened_file = std::fopen(filenameBuffer->data(), "r+");
            DoFinally closeOp( [&opened_file]() {
                std::fclose(opened_file);
            });

            if (!opened_file) {
                Logger::log(LogLevel::Warning, "File doesn't exist, trying to open tmp file: %s", filenameBuffer->data());
                opened_file = openTmpFile(filename, false);

                if (opened_file == nullptr) {
                    Logger::log(LogLevel::Warning, "Couldn't open tmp file for: %s", filenameBuffer->data());
                    return ESP_FAIL;
                }
            }

            std::fseek(opened_file, 0, SEEK_END);
            auto file_size = std::ftell(opened_file);

            if (file_size != sizeof(T)) {
                Logger::log(LogLevel::Warning, "File size of %s is %d and that isn't the correct size %d", 
                    SettingType::StorageName,
                    static_cast<int>(file_size),
                    static_cast<int>(sizeof(T)));
                return ESP_FAIL;
            }

            std::fseek(opened_file, 0, SEEK_SET);
            const auto read_size = std::fread(reinterpret_cast<void *>(&value), sizeof(T), 1, opened_file);

            Logger::log(LogLevel::Info, "Read %d bytes from the sd card", read_size * sizeof(T));

            return read_size == 1;
        }

        template<IsByteCopyable T>
        esp_err_t storeToFilesystem(const char *filename, const T &value) {
            if (!m_initialized) {
                return ESP_FAIL;
            }

            Logger::log(LogLevel::Info, "Writing to filesystem");

            auto target_file = openTmpFile(filename, true);

            if (!target_file) {
                Logger::log(LogLevel::Info, "Couldn't open tmp file");
                return ESP_FAIL;
            }

            std::rewind(target_file);

            Logger::log(LogLevel::Info, "Opened tmp file");

            auto written_size = std::fwrite(reinterpret_cast<const void *>(&value), sizeof(T), 1, target_file);
            std::fclose(target_file);
            Logger::log(LogLevel::Info, "Wrote %d bytes to the filesystem", written_size * sizeof(T));

            int rename_result = -1;
            if (written_size == 1) {
                auto filenameBuffer = SmallerBufferPoolType::get_free_buffer();
                auto tmp_filenameBuffer = SmallerBufferPoolType::get_free_buffer();
                // TODO: check results of both methods
                copyFilenameToBuffer(tmp_filenameBuffer, filename, ".tmp");
                copyFilenameToBuffer(filenameBuffer, filename);
                std::remove(filenameBuffer->data());
                Logger::log(LogLevel::Info, "Renaming %s to %s", tmp_filenameBuffer->data(), filenameBuffer->data());
                rename_result = std::rename(tmp_filenameBuffer->data(), filenameBuffer->data());
            } else {
                Logger::log(LogLevel::Warning, "Setting couldn't be written skipping renaming to real file to avoid issues");
            }

            if (rename_result < 0) {
                Logger::log(LogLevel::Error, "Couldn't rename file");
            }

            if (written_size != 1 || rename_result < 0) {
                return false;
            }

            return true;
        }
    
        bool m_initialized = false;
        std::optional<FilesystemType> m_filesystem = std::nullopt;
        std::mutex instanceMutex;
        SettingType m_setting;
};