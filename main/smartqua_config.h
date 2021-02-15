#pragma once

#include <cstdint>

#ifndef NAME_LENGTH
#define NAME_LENGTH 16
#endif

#ifndef MAX_PINS_FOR_DEVICE
#define MAX_PINS_FOR_DEVICE 4
#endif

#ifndef MAX_DEVICE_CONFIG_SIZE
#define MAX_DEVICE_CONFIG_SIZE 32
#endif

#ifndef MAX_NUM_DEVICES
#define MAX_NUM_DEVICES 16
#endif

#ifndef MAX_NUM_TIMERS
#define MAX_NUM_TIMERS 16
#endif

#ifndef MAX_ACTION_NAME_LENGTH
#define MAX_ACTION_NAME_LENGTH 16
#endif

static inline constexpr uint8_t name_length = NAME_LENGTH;

static inline constexpr uint8_t max_pins_for_device = MAX_PINS_FOR_DEVICE;

static inline constexpr uint8_t max_num_devices = MAX_NUM_DEVICES;

static inline constexpr uint8_t device_config_size = MAX_DEVICE_CONFIG_SIZE;

static inline constexpr uint8_t max_num_timers = MAX_NUM_TIMERS;

static inline constexpr uint8_t max_action_name_length = MAX_ACTION_NAME_LENGTH;