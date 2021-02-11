#pragma once

#include <cstdint>

#ifndef NAME_LENGTH
#define NAME_LENGTH 16
#endif

#ifndef MAX_PINS_FOR_DEVICE
#define MAX_PINS_FOR_DEVICE 4
#endif

static inline constexpr uint8_t name_length = NAME_LENGTH;

static inline constexpr uint8_t max_pins_for_device = MAX_PINS_FOR_DEVICE;