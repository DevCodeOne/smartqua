#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

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
#include "utils/utils.h"

template<ConstexprPath Path>
class LocalFlashStorage {
    public:
    static constexpr const char *MountPointPath = Path.value;

    LocalFlashStorage(LocalFlashStorage &&other) = default;
    LocalFlashStorage(const LocalFlashStorage &other) = delete;

    LocalFlashStorage &operator=(LocalFlashStorage &&other) = default;
    LocalFlashStorage &operator=(const LocalFlashStorage &other) = delete;

    ~LocalFlashStorage(){
        // esp_vfs_fat_spiflash_unmount_rw_wl
        // esp_partition_deregister_external
        // spi_bus_remove_device
    }

    static std::optional<LocalFlashStorage> create() {
        if (m_flashPartition != nullptr) {
            // already initialized
            return LocalFlashStorage();
        }

        const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, MountPointPath + 1);

        if (partition == nullptr) {
            Logger::log(LogLevel::Error, "Couldn't find partition");
            return std::nullopt;
        }

        wl_handle_t handleOut;
        esp_vfs_fat_mount_config_t fatMountConfig{
            .format_if_mount_failed = true,
            .max_files = 3,
            .allocation_unit_size = 0
        };

        auto result = esp_vfs_fat_spiflash_mount_rw_wl(MountPointPath, MountPointPath + 1, &fatMountConfig, &handleOut);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Error, "Couldn't mount fat filesystem : %s", esp_err_to_name(result));
        }

        initializeValues(partition, handleOut);
        return LocalFlashStorage();
    }

    private:
    LocalFlashStorage() { }

    static bool initializeValues(const esp_partition_t *flashPartition, wl_handle_t flashWearLevelHandle) {
        m_flashPartition = flashPartition;
        m_flashWearLevelHandle = flashWearLevelHandle;

        return true;
    }

    static inline const esp_partition_t *m_flashPartition = nullptr;
    static inline wl_handle_t m_flashWearLevelHandle;

    static inline DoFinally cleanupOnDestroy {
        []() {
            // TODO: do cleanup here
            return;
        }
    };
};