#pragma once

#include <cstdint>

#include "storage/store.h"
#include "rest/scale_rest.h"
#include "rest/pwm_rest.h"
#include "rest/devices_rest.h"

#include "drivers/ds18x20_driver.h"

using device_settings_type = device_settings<ds18x20_driver>;

static inline store<
    single_store<scale_settings, nvs_setting<scale_settings::trivial_representation> >,
    single_store<pwm_settings, nvs_setting<pwm_settings::trivial_representation> >,
    single_store<device_settings_type, nvs_setting<device_settings_type::trivial_representation> >
    > global_store;