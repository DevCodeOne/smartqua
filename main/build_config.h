#pragma once

#include <limits>

#ifndef NAME_LENGTH
#define NAME_LENGTH 16
#endif

#ifndef MAX_DEVICE_CONFIG_SIZE
#define MAX_DEVICE_CONFIG_SIZE 128
#endif

#ifndef MAX_NUM_DEVICES
#define MAX_NUM_DEVICES 12
#endif

#ifndef MAX_NUM_TIMERS
#define MAX_NUM_TIMERS 16
#endif

#ifndef MAX_TASK_POOL_SIZE
#define MAX_TASK_POOL_SIZE 32
#endif

#ifndef MAX_STAT_SIZE
#define MAX_STAT_SIZE 8
#endif

#ifndef MAX_SETTING_SIZE
#define MAX_SETTING_SIZE 12
#endif

#ifndef MAX_SAMPLE_SIZE
#define MAX_SAMPLE_SIZE 16
#endif

#ifndef MAX_ARGUMENT_LENGTH
#define MAX_ARGUMENT_LENGTH 32
#endif

#ifndef REMOTE_SETTING_HOST
#define REMOTE_SETTING_HOST "nextcloud.fritz.box:9000"
// #define REMOTE_SETTING_HOST "chris-pc.fritz.box:9000"
#endif

#ifndef DEFAULT_HOST_NAME
#define DEFAULT_HOST_NAME CONFIG_LWIP_LOCAL_HOSTNAME
#endif

#define RPI0        0
#define ESP32       1

#ifndef TARGET_DEVICE
#define TARGET_DEVICE ESP32
#endif


static inline constexpr uint8_t name_length = NAME_LENGTH;

static inline constexpr uint8_t max_num_devices = MAX_NUM_DEVICES;

static inline constexpr uint8_t device_config_size = MAX_DEVICE_CONFIG_SIZE;

static inline constexpr uint8_t max_num_timers = MAX_NUM_TIMERS;

static inline constexpr uint8_t max_task_pool_size = MAX_TASK_POOL_SIZE;

static inline constexpr uint8_t max_stat_size = MAX_STAT_SIZE;

static inline constexpr uint8_t max_sample_size = MAX_SAMPLE_SIZE;

static inline constexpr uint8_t max_setting_size = MAX_SETTING_SIZE;

static inline constexpr char remote_setting_host [] = REMOTE_SETTING_HOST;

static inline constexpr char default_host_name [] = DEFAULT_HOST_NAME;

static inline uint8_t constexpr MaxArgumentLength = MAX_ARGUMENT_LENGTH;

static constexpr unsigned int InvalidDeviceId = std::numeric_limits<unsigned int>::max();

static inline constexpr size_t num_large_buffers = 3;

static inline constexpr size_t large_buffer_size = 2048;

static inline constexpr size_t stack_size = 6 * 4096;

constexpr static inline auto sdaDefaultPin = 21;

constexpr static inline auto sclDefaultPin = 22;

#include "utils/logger.h"

using Logger = ApplicationLogger<VoidSink>;

#include "utils/large_buffer_pool.h"

// TODO: make configurable
using LargeBufferPoolType = LargeBufferPool<num_large_buffers, large_buffer_size, BufferLocation::heap>;