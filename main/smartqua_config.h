#pragma once

#include <cstdint>

#include "build_config.h"
#include "utils/logger.h"

using Logger = ApplicationLogger<VoidSink>;

#include "storage/flash_storage.h"

using DefaultFlashStorage = FlashStorage<"/values">;