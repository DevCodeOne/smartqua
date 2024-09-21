#pragma once

#include <shared_mutex>

#include "storage/nvs_flash_utils.h"
#include "driver/sdmmc_types.h"

class sd_filesystem {
    public:
        sd_filesystem();
        ~sd_filesystem();

        operator bool() const;

        static inline constexpr char mount_point[] = "/external";
    private:

        static void initialize();
        // TODO: incorperate somehow
        static void deinit();

        static inline std::once_flag initializeFlag;
        static inline bool _is_initialized = false;
        static inline struct {
            sdmmc_card_t *card;
        } _sd_data{ .card = nullptr };
};