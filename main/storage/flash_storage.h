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

#include "smartqua_config.h"
#include "utils/filesystem_utils.h"
#include "utils/utils.h"

template<ConstexprPath Path>
class FlashStorage {
    public:
    static constexpr const char *MountPointPath = Path.value;

    FlashStorage(FlashStorage &&other) = default;
    FlashStorage(const FlashStorage &other) = delete;

    FlashStorage &operator=(FlashStorage &&other) = default;
    FlashStorage &operator=(const FlashStorage &other) = delete;

    ~FlashStorage(){
        // esp_vfs_fat_spiflash_unmount_rw_wl
        // esp_partition_deregister_external
        // spi_bus_remove_device
    }

    static std::optional<FlashStorage> create() {
        if (m_flashChip != nullptr) {
            // already initialized
            return FlashStorage();
        }

        spi_bus_config_t spi_config{
            .mosi_io_num = 23,
            .miso_io_num = 19,
            .sclk_io_num = 18,
        };
        auto result = spi_bus_initialize(SPI3_HOST, &spi_config, SPI_DMA_CH1);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Error, "Couldn't initialize spi bus");
            return std::nullopt;
        }

        esp_flash_t *flashChip = nullptr;
        esp_flash_spi_device_config_t flashConfig{
            .host_id = SPI3_HOST,
            .cs_io_num = 5,
            .io_mode = SPI_FLASH_DIO,
            .input_delay_ns = 0,
            .freq_mhz = ESP_FLASH_40MHZ,
        };

        spi_bus_add_flash_device(&flashChip, &flashConfig);

        result = esp_flash_init(flashChip);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Error, "Couldn't initialize flash chip");
            return std::nullopt;
        }

        const esp_partition_t *partition = nullptr;

        // 8M
        result = esp_partition_register_external(flashChip, 0, 8 * 1024 * 1024, MountPointPath, 
            ESP_PARTITION_TYPE_DATA, 
            ESP_PARTITION_SUBTYPE_DATA_FAT, 
            &partition);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Error, "Couldn't create partition");
            return std::nullopt;
        }

        wl_handle_t handleOut;
        esp_vfs_fat_mount_config_t fatMountConfig{
            .format_if_mount_failed = true,
            .max_files = 4,
            .allocation_unit_size = 0
        };

        result = esp_vfs_fat_spiflash_mount_rw_wl(MountPointPath, MountPointPath, &fatMountConfig, &handleOut);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Error, "Couldn't mount fat filesystem : %s", esp_err_to_name(result));
        }

        initializeValues(flashChip, partition, handleOut);
        return FlashStorage();
    }

    private:
    FlashStorage() { }

    static bool initializeValues(esp_flash_t *flashChip, const esp_partition_t *flashPartition, wl_handle_t flashWearLevelHandle) {
        m_flashChip = flashChip;
        m_flashPartition = flashPartition;
        m_flashWearLevelHandle = flashWearLevelHandle;

        return true;
    }

    static inline esp_flash_t *m_flashChip = nullptr;
    static inline const esp_partition_t *m_flashPartition = nullptr;
    static inline wl_handle_t m_flashWearLevelHandle;

    static inline DoFinally cleanupOnDestroy {
        []() {
            // TODO: do cleanup here
            return;
        }
    };
};