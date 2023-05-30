#pragma once

#include <cstdint>

#include "build_config.h"
#include "utils/logger.h"

using Logger = ApplicationLogger<VoidSink>;

#include "storage/flash_storage.h"
#include "storage/local_flash_storage.h"

// using DefaultFlashStorage = FlashStorage<"/values">;
using DefaultFlashStorage = LocalFlashStorage<"/values">;