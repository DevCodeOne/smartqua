#pragma once

#include <cstdint>

#ifndef NAME_LENGTH
#define NAME_LENGTH 32
#endif

#ifndef MAX_PINS_FOR_DEVICE
#define MAX_PINS_FOR_DEVICE 4
#endif

#ifndef MAX_DEVICE_CONFIG_SIZE
#define MAX_DEVICE_CONFIG_SIZE 128
#endif

#ifndef MAX_NUM_DEVICES
#define MAX_NUM_DEVICES 16
#endif

#ifndef MAX_NUM_TIMERS
#define MAX_NUM_TIMERS 16
#endif

#ifndef MAX_ACTION_PAYLOAD_LENGTH
#define MAX_ACTION_PAYLOAD_LENGTH 64
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

#ifndef REMOTE_SETTING_HOST
#define REMOTE_SETTING_HOST "nextcloud.fritz.box:9000"
#endif

#define RPI0        0
#define ESP32       1

#ifndef TARGET_DEVICE
#define TARGET_DEVICE ESP32
#endif

static inline constexpr uint8_t name_length = NAME_LENGTH;

static inline constexpr uint8_t max_pins_for_device = MAX_PINS_FOR_DEVICE;

static inline constexpr uint8_t max_num_devices = MAX_NUM_DEVICES;

static inline constexpr uint8_t device_config_size = MAX_DEVICE_CONFIG_SIZE;

static inline constexpr uint8_t max_num_timers = MAX_NUM_TIMERS;

static inline constexpr uint8_t max_action_payload_length = MAX_ACTION_PAYLOAD_LENGTH;

static inline constexpr uint8_t max_task_pool_size = MAX_TASK_POOL_SIZE;

static inline constexpr uint8_t max_stat_size = MAX_STAT_SIZE;

static inline constexpr uint8_t max_setting_size = MAX_SETTING_SIZE;

static inline constexpr char remote_setting_host [] = REMOTE_SETTING_HOST;

#include "utils/logger.h"

using Logger = ApplicationLogger<SdCardSink, HttpLogSink>;