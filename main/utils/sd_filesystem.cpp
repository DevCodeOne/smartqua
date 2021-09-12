#include "sd_filesystem.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

sd_filesystem::sd_filesystem() {
    std::call_once(initializeFlag, []() {
        ESP_LOGI("SD_Filesystem", "Initializing sd card");

        esp_vfs_fat_sdmmc_mount_config_t mount_config {
            .format_if_mount_failed = true,
            .max_files = 32,
            .allocation_unit_size = 16 * 1024
        };

        gpio_set_pull_mode((gpio_num_t)15, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode((gpio_num_t)2, GPIO_PULLUP_ONLY);
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = 1;

        ESP_LOGI("SD_Filesystem", "Trying to mount sd card");

        auto result = esp_vfs_fat_sdmmc_mount(sd_filesystem::mount_point, &host, &slot_config, &mount_config, &_sd_data.card);

        if (result != ESP_OK) {
            ESP_LOGE("SD_Filesystem", "Failed to initialize card");
            return;
        }

        _is_initialized = true;

        sdmmc_card_print_info(stdout, _sd_data.card);

        ESP_LOGI("SD_Filesystem", "Initialized sd card");
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