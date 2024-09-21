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
class DummyStorage {
    public:
    static constexpr char MountPointPath[]{Path.value};

    DummyStorage(DummyStorage &&other) = default;
    DummyStorage(const DummyStorage &other) = delete;

    DummyStorage &operator=(DummyStorage &&other) = default;
    DummyStorage &operator=(const DummyStorage &other) = delete;

    ~DummyStorage() = default;

    static std::optional<DummyStorage> create() { return DummyStorage(); }

    private:
    DummyStorage() = default;
};