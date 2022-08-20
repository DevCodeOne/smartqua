#pragma once

#include <cstdint>

#include "storage/store.h"
#include "storage/settings.h"
#include "rest/scale_rest.h"
#include "actions/device_actions.h"
#include "actions/soft_timer_actions.h"
#include "actions/stats_actions.h"
#include "actions/setting_actions.h"
#include "utils/large_buffer_pool.h"
#include "utils/logger.h"

#include "drivers/ds18x20_driver.h"
#include "drivers/pwm.h"
#include "drivers/scale.h"
#include "drivers/dac_driver.h"
#include "drivers/lamp_driver.h"
#include "drivers/q30_driver.h"
#include "drivers/setting_types.h"

template<typename SettingType>
using DefaultSaveType = FilesystemSetting<SettingType>;

template<typename SettingType>
using RemoteSaveType = RestRemoteSetting<SettingType>;

using DeviceSettingsType = DeviceSettings<max_num_devices, Ds18x20Driver, PwmDriver, loadcell, DacDriver, LampDriver, Q30Driver>;
using SoftTimerSettingsType = soft_timer_settings<max_num_timers>;
using StatCollectionType = StatCollection<max_stat_size>;
// using SettingType = settings<max_setting_size>;

// TODO: add settings
extern Store<
    SingleSetting<DeviceSettingsType, RemoteSaveType<DeviceSettingsType::TrivialRepresentationType> >,
    SingleSetting<SoftTimerSettingsType, DefaultSaveType<SoftTimerSettingsType::trivial_representation> >,
    SingleSetting<StatCollectionType, DefaultSaveType<StatCollectionType::trivial_representation> >
    // SingleSetting<SettingType, DefaultSafeType<SettingType::trivial_representation> >
    > global_store;

// TODO: make configurable
using LargeBufferPoolType = LargeBufferPool<4, 2048, BufferLocation::stack>;