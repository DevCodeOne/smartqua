#pragma once

#include <cstdint>
#include <optional>

#include "storage/store.h"
#include "storage/flash_storage.h"
#include "storage/settings.h"
#include "rest/scale_rest.h"
#include "actions/device_actions.h"
#include "actions/stats_actions.h"
#include "actions/setting_actions.h"
#include "utils/large_buffer_pool.h"
#include "utils/logger.h"

#include "drivers/ds18x20_driver.h"
#include "drivers/ads111x_driver.h"
#include "drivers/pwm.h"
#include "drivers/scale_driver.h"
#include "drivers/dac_driver.h"
#include "drivers/schedule_driver.h"
#include "drivers/q30_driver.h"
#include "drivers/stepper_dosing_pump_driver.h"
#include "drivers/setting_types.h"

#include "smartqua_config.h"

template<typename SettingType, ConstexprPath path>
using LocalSaveType = FilesystemSetting<SettingType, path, DefaultFlashStorage>;

template<typename SettingType>
using RemoteSaveType = RestRemoteSetting<SettingType>;

using DeviceSettingsType = DeviceSettings<max_num_devices, 
                                        Ds18x20Driver, 
                                        Ads111xDriver,
                                        PwmDriver, 
                                        LoadcellDriver,
                                        DacDriver,
                                        ScheduleDriver, 
                                        //Q30Driver, 
                                        StepperDosingPumpDriver>;
using StatCollectionType = StatCollection<max_stat_size>;
// using SettingType = settings<max_setting_size>;

// TODO: add settings
using GlobalStoreType = Store<
    SingleSetting<DeviceSettingsType, LocalSaveType<DeviceSettingsType::TrivialRepresentationType, "devices.bin"> >,
    SingleSetting<StatCollectionType, LocalSaveType<StatCollectionType::trivial_representation, "stats.bin"> >
    // SingleSetting<SettingType, DefaultSafeType<SettingType::trivial_representation> >
    >;
extern std::optional<GlobalStoreType> global_store;

// TODO: make configurable
using LargeBufferPoolType = LargeBufferPool<num_large_buffers, large_buffer_size, BufferLocation::heap>;