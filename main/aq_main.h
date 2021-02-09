#pragma once

#include <cstdint>

#include "store.h"
#include "scale_rest.h"
#include "pwm_rest.h"

static inline store<
    single_store<scale_settings, nvs_setting<scale_settings::trivial_representation>>,
    single_store<pwm_settings, nvs_setting<pwm_settings::trivial_representation>>> global_store;