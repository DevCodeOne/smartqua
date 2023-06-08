#include "sd_filesystem.h"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "hal/gpio_types.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "utils/logger.h"
#include "build_config.h"

sd_filesystem::sd_filesystem() {
    std::call_once(initializeFlag, []() {
        Logger::log(LogLevel::Info, "Initializing sd card");
        const esp_vfs_fat_sdmmc_mount_config_t mount_config {
                .format_if_mount_failed = true,
                .max_files = 32,
                .allocation_unit_size = 16 * 1024
            };


        //const auto tryHostWithPins = [&mount_config](int numPins) {
        //    gpio_set_pull_mode((gpio_num_t)15, GPIO_PULLUP_ONLY);
        //    gpio_set_pull_mode((gpio_num_t)2, GPIO_PULLUP_ONLY);
        //    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        //    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
        //    host.command_timeout_ms = 2000;
        //    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        //    slot_config.width = numPins;

        //    Logger::log(LogLevel::Info, "Trying to mount sd card");

        //    auto result = esp_vfs_fat_sdmmc_mount(sd_filesystem::mount_point, &host, &slot_config, &mount_config, &_sd_data.card);

        //    if (result != ESP_OK) {
        //        gpio_set_pull_mode((gpio_num_t)15, GPIO_FLOATING);
        //        gpio_set_pull_mode((gpio_num_t)2, GPIO_FLOATING);
        //    }

        //    return result;

        //};

        const auto trySpi = [&mount_config]() {
            sdmmc_host_t host = SDSPI_HOST_DEFAULT();
            host.command_timeout_ms = 2000;
            spi_bus_config_t bus_cfg = {
                //.mosi_io_num = 23,
                //.miso_io_num = 19,
                .mosi_io_num = 19,
                .miso_io_num = 23,
                .sclk_io_num = 18,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = 4000
            };

            Logger::log(LogLevel::Info, "Trying to mount sd card");
            auto initResult = spi_bus_initialize(spi_host_device_t(host.slot), &bus_cfg, SPI_DMA_CH_AUTO);

            if (initResult != ESP_OK) {
                Logger::log(LogLevel::Warning, "Couldn't create spi bus");
                return initResult;
            }

            sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
            slot_config.gpio_cs = gpio_num_t::GPIO_NUM_15;
            slot_config.host_id = spi_host_device_t(host.slot);

            return esp_vfs_fat_sdspi_mount(sd_filesystem::mount_point, &host, &slot_config, &mount_config, &_sd_data.card);

        };
        
        esp_err_t initResult = ESP_FAIL;


        //if ((initResult = tryHostWithPins(4)) != ESP_OK) {
        //    Logger::log(LogLevel::Warning, "Failed to initialize card with slot_width 4 ...");
        //}

        //if (initResult != ESP_OK && (initResult = tryHostWithPins(1)) != ESP_OK) {
        //    Logger::log(LogLevel::Warning, "Failed to initialize card with slot_width 1 ...");

        //}

        if (initResult != ESP_OK && (initResult = trySpi()) != ESP_OK) {
            Logger::log(LogLevel::Warning, "Failed to initialize card with spi giving up ...");
        }

        if (initResult != ESP_OK) {
            Logger::log(LogLevel::Warning, "Failed to initialize card");
            return;
        }

        _is_initialized = true;

        sdmmc_card_print_info(stdout, _sd_data.card);

        Logger::log(LogLevel::Info, "Initialized sd card");
    });
}

// TODO: deinitialize
sd_filesystem::~sd_filesystem() {
}

sd_filesystem::operator bool() const {
    return _is_initialized;
}

void sd_filesystem::initialize() {
}