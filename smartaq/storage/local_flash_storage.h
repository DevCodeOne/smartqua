#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>

#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "hal/spi_flash_types.h"
#include "hal/spi_types.h"
#include "spi_flash_mmap.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"

#include "build_config.h"
#include "utils/filesystem_utils.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "wear_levelling.h"

template<ConstexprPath Path>
class LocalFlashStorage {
    public:
    static constexpr ConstexprPath path = Path;

    LocalFlashStorage(LocalFlashStorage &&other) = default;
    LocalFlashStorage(const LocalFlashStorage &other) = delete;

    LocalFlashStorage &operator=(LocalFlashStorage &&other) = default;
    LocalFlashStorage &operator=(const LocalFlashStorage &other) = delete;

    ~LocalFlashStorage(){
        // This needs instance counting e.g. with std::shared_ptr
        // if (m_flashWearLevelHandle) {
        //     esp_vfs_fat_spiflash_unmount_rw_wl(MountPointPath, *m_flashWearLevelHandle);
        // }
    }

    template<typename DataSourceLambda>
    static bool unMountWriteBackupAndMount(DataSourceLambda &dataSource) {
        if (m_flashWearLevelHandle) {
            esp_vfs_fat_spiflash_unmount_rw_wl(Path.value, *m_flashWearLevelHandle);
        }

        const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, Path.value + 1);

        if (partition == nullptr) {
            return false;
        }

        auto eraseResult = esp_partition_erase_range(partition, 0, partition->size);

        if (eraseResult != ESP_OK) {
            return false;
        }

        auto buffer = SmallerBufferPoolType::get_free_buffer();
        using LambdaReturnType = decltype(dataSource(buffer));
        static_assert(std::is_integral_v<LambdaReturnType>, "DataSource Return type has to be integral");
        LambdaReturnType toWrite = 0;
        size_t written = 0;
        while ((toWrite = dataSource(buffer)) > 0) {
            auto writeResult = esp_partition_write_raw(partition, written, buffer->data(), toWrite);

            if (writeResult != ESP_OK) {
                return false;
            }

            written += toWrite;
        }

        uint8_t id[32];
        esp_partition_get_sha256(partition, id);

        char output[128];
        int offset = 0;
        for (int i = 0; i < 32; ++i) {
            offset += snprintf(output + offset, (output + sizeof(output)) - (output + offset), "%02x", id[i]);
        }
        ESP_LOGI("written partition", "%.*s", 128, output);


        Logger::log(LogLevel::Info, "PartitionSize : %u ", partition->size);
        Logger::log(LogLevel::Info, "Written : %u ", written);

        auto handleOut = mountPartition(partition);

        if (!handleOut) {
            return false;
        }

        initializeValues(partition, *handleOut);

        return true;
    }

    static std::optional<LocalFlashStorage> create() {
        if (m_flashWearLevelHandle) {
            // already initialized
            return LocalFlashStorage();
        }

        const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, Path.value + 1);

        if (partition == nullptr) {
            Logger::log(LogLevel::Error, "Couldn't find partition %.*s", Path.length, Path.value);
            return std::nullopt;
        }

        auto handleOut = mountPartition(partition);

        if (!handleOut) {
            return std::nullopt;
        }

        initializeValues(partition, *handleOut);
        Logger::log(LogLevel::Info, "Initialized Filesystem %.*s", Path.length, Path.value);
        return LocalFlashStorage();
    }

    private:
    LocalFlashStorage() { }

    static std::optional<wl_handle_t> mountPartition(const esp_partition_t *partition) {
        wl_handle_t handleOut;
        esp_vfs_fat_mount_config_t fatMountConfig{
            .format_if_mount_failed = true,
            .max_files = 12,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
            .use_one_fat = false,
        };

        auto result = esp_vfs_fat_spiflash_mount_rw_wl(Path.value, Path.value + 1, &fatMountConfig, &handleOut);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Error, "Couldn't mount fat filesystem : %s", esp_err_to_name(result));
            return std::nullopt;
        }

        const auto filesystemCheck = writeTestFile("test.tmp", "test");
        switch (filesystemCheck) {
            case FileSystemStatus::NoOpen:
            Logger::log(LogLevel::Error, "Couldn't open test file, partition probably doesn't work");
            break;
            case FileSystemStatus::NoWrite:
            Logger::log(LogLevel::Error, "Couldn't write to test file, partition probably doesn't work");
            break;
            case FileSystemStatus::NoValidate:
            Logger::log(LogLevel::Error, "Couldn't validate test file, partition probably doesn't work");
            break;
            case FileSystemStatus::NoRemove:
            Logger::log(LogLevel::Error, "Couldn't remove test file, partition probably doesn't work");
            break;
            case FileSystemStatus::Ok:
            Logger::log(LogLevel::Debug, "Mounted partition with label %.*s at %.*s", Path.length - 1, Path.value + 1, Path.length, Path.value);
        }

        if (filesystemCheck != FileSystemStatus::Ok) {
            return std::nullopt;
        }


        return std::make_optional(handleOut);
    }

    static bool initializeValues(const esp_partition_t *flashPartition, wl_handle_t flashWearLevelHandle) {
        m_flashPartition = flashPartition;
        m_flashWearLevelHandle = flashWearLevelHandle;

        return true;
    }

    static inline const esp_partition_t *m_flashPartition = nullptr;
    static inline std::optional<wl_handle_t> m_flashWearLevelHandle = std::nullopt;

    static inline DoFinally cleanupOnDestroy {
        []() {
            // TODO: do cleanup here
            return;
        }
    };
};