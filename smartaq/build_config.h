#pragma once

#include "drivers/hal/device_config.h"
#include "utils/buffer_types.h"

#include "utils/logger.h"

#if TARGET_DEVICE == ESP32
    #include "utils/esp/esp_logger_utils.h"
    using Logger = ApplicationLogger<EspIdfBackend, VoidSink>;
#else
    using Logger = ApplicationLogger<PrintfBackend, VoidSink>;
#endif

// TODO: Create system agnostic version of this
#if TARGET_DEVICE == ESP32
    #include "utils/large_buffer_pool.h"
    // TODO: make configurable
    using LargeBufferPoolType = LargeBufferPool<num_large_buffers, large_buffer_size, BufferLocation::heap>;
    using SmallerBufferPoolType = LargeBufferPool<20, 256, BufferLocation::heap>;

    #include "utils/task_pool.h"

    using MainTaskPool = TaskPool<max_task_pool_size>;


    // Device specific section
    #if __has_include("sdkconfig.h")
    #include "sdkconfig.h"
    #endif
#endif

#ifndef DEFAULT_HOST_NAME
#ifdef CONFIG_LWIP_LOCAL_HOSTNAME
#define DEFAULT_HOST_NAME CONFIG_LWIP_LOCAL_HOSTNAME
#else
#define DEFAULT_HOST_NAME "devboard"
#endif
#endif

#if __has_include("driver/dac.h") && !defined(CONFIG_IDF_TARGET_ESP32S3)
    #define ENABLE_DAC_DRIVER 1
#endif

static inline constexpr char default_host_name [] = DEFAULT_HOST_NAME;
