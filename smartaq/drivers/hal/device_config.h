#pragma once

#pragma once

#include <limits>
#include <cstddef>
#include <cstdint>

#define USE_PSRAM 1

#ifndef NAME_LENGTH
#define NAME_LENGTH 16
#endif

#ifndef MAX_DEVICE_CONFIG_SIZE
#define MAX_DEVICE_CONFIG_SIZE 255
#endif

#ifdef USE_PSRAM
#define MORE_RAM 1
#endif

#if MORE_RAM == 1
    #ifndef MAX_NUM_DEVICES
    #define MAX_NUM_DEVICES 24
    #endif
#else
    #ifndef MAX_NUM_DEVICES
    #define MAX_NUM_DEVICES 14
    #endif
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
#define MAX_ARGUMENT_LENGTH 16
#endif

#ifndef REMOTE_SETTING_HOST
#define REMOTE_SETTING_HOST "nextcloud.fritz.box:9000"
// #define REMOTE_SETTING_HOST "chris-pc.fritz.box:9000"
#endif

#define RPI0        0
#define ESP32       1
#define OTHER       2

#ifndef TARGET_DEVICE
#define TARGET_DEVICE ESP32
#endif

#ifndef MAX_SCHEDULE_CHANNEL_NAME_LENGTH
#define SCHEDULE_MAX_CHANNEL_NAME_LENGTH 8
#endif

#ifndef SCHEDULE_MAX_NUM_CHANNELS
#define SCHEDULE_MAX_NUM_CHANNELS 4
#endif

#ifndef SCHEDULE_MAX_NUM_CHANNELS
#define SCHEDULE_MAX_NUM_CHANNELS 4
#endif

static inline constexpr uint8_t schedule_max_channel_name_length = SCHEDULE_MAX_CHANNEL_NAME_LENGTH;

static inline constexpr uint8_t schedule_max_num_channels = SCHEDULE_MAX_NUM_CHANNELS;

static inline constexpr uint8_t name_length = NAME_LENGTH;

static inline constexpr uint8_t max_num_devices = MAX_NUM_DEVICES;

static inline constexpr uint8_t device_config_size = MAX_DEVICE_CONFIG_SIZE;

static inline constexpr uint8_t max_num_timers = MAX_NUM_TIMERS;

static inline constexpr uint8_t max_task_pool_size = MAX_TASK_POOL_SIZE;

static inline constexpr uint8_t max_stat_size = MAX_STAT_SIZE;

static inline constexpr uint8_t max_sample_size = MAX_SAMPLE_SIZE;

static inline constexpr uint8_t max_setting_size = MAX_SETTING_SIZE;

static inline constexpr char remote_setting_host [] = REMOTE_SETTING_HOST;

static inline uint8_t constexpr MaxArgumentLength = MAX_ARGUMENT_LENGTH;

static constexpr unsigned int InvalidDeviceId = std::numeric_limits<unsigned int>::max();

static inline constexpr size_t num_large_buffers = 12;

static inline constexpr size_t large_buffer_size = 2048;

constexpr static inline auto sdaDefaultPin = 15;

constexpr static inline auto sclDefaultPin = 16;
