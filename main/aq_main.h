#pragma once

#include <cstdint>

#include "storage/store.h"
#include "rest/scale_rest.h"
#include "actions/device_actions.h"
#include "actions/soft_timer_actions.h"

#include "drivers/ds18x20_driver.h"
#include "drivers/pwm.h"

using device_settings_type = device_settings<ds18x20_driver, pwm>;

// TODO: add timers
extern store<
    single_store<scale_settings, nvs_setting<scale_settings::trivial_representation> >,
    single_store<device_settings_type, nvs_setting<device_settings_type::trivial_representation> >,
    single_store<soft_timer_settings, nvs_setting<soft_timer_settings::trivial_representation> >
    > global_store;