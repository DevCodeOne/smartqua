#pragma once

#include <cstdint>

#include "storage/store.h"
#include "storage/settings.h"
#include "rest/scale_rest.h"
#include "actions/device_actions.h"
#include "actions/soft_timer_actions.h"
#include "actions/stats_actions.h"
#include "utils/large_buffer_pool.h"

#include "drivers/ds18x20_driver.h"
#include "drivers/pwm.h"
#include "drivers/scale.h"
#include "drivers/setting_types.h"

using device_settings_type = device_settings<max_num_devices, ds18x20_driver, pwm, loadcell>;
using soft_timer_settings_type = soft_timer_settings<max_num_timers>;
using stat_collection_type = stat_collection<max_stat_size>;

// TODO: add settings
extern store<
    single_store<device_settings_type, sd_card_setting<device_settings_type::trivial_representation> >,
    single_store<soft_timer_settings_type, sd_card_setting<soft_timer_settings_type::trivial_representation> >,
    single_store<stat_collection_type, sd_card_setting<stat_collection_type::trivial_representation> >
    > global_store;

// TODO: make configurable
using large_buffer_pool_type = large_buffer_pool<8, 4096>;