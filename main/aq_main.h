#pragma once

#include <cstdint>

#include "storage/store.h"
#include "storage/settings.h"
#include "rest/scale_rest.h"
#include "actions/device_actions.h"
#include "actions/soft_timer_actions.h"

#include "drivers/ds18x20_driver.h"
#include "drivers/pwm.h"
#include "drivers/scale.h"

using device_settings_type = device_settings<ds18x20_driver, pwm, loadcell>;

// TODO: add settings
extern store<
    single_store<device_settings_type, sd_card_setting<device_settings_type::trivial_representation> >,
    single_store<soft_timer_settings, sd_card_setting<soft_timer_settings::trivial_representation> >
    > global_store;