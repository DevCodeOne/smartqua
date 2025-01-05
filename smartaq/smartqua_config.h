#pragma once

#include <cstdint>
#include <memory>

#include "build_config.h"

#include "storage/store.h"
#include "storage/flash_storage.h"
#include "storage/settings.h"
#include "actions/device_actions.h"
#include "actions/stats_actions.h"
#include "actions/setting_actions.h"
#include "utils/esp/idf_utils.h"

#include "drivers/ds18x20_driver.h"
#include "drivers/ads111x_driver.h"
#include "drivers/pin_driver.h"
#include "drivers/ph_probe_driver.h"
#include "drivers/pcf8575_driver.h"
#include "drivers/scale_driver.h"
#include "drivers/dac_driver.h"
#include "drivers/schedule_driver.h"
#include "drivers/pico_device_driver.h"
#include "drivers/drv8825_driver.h"
#include "drivers/stepper_dosing_pump_driver.h"
#include "drivers/switch_driver.h"
#include "drivers/setting_types.h"
#include "drivers/dhtxx_driver.h"
#include "drivers/bme280_driver.h"

#include "storage/flash_storage.h"
#include "storage/dummy_storage.h"
#include "storage/local_flash_storage.h"

// using DefaultStorage = FlashStorage<"/values">;
using DefaultStorage = LocalFlashStorage<ConstexprPath("/values")>;
// using DefaultStorage = DummyStorage<"/values">;

template<typename SettingType, ConstexprPath path>
using LocalSaveType = FilesystemSetting<SettingType, path, DefaultStorage>;

template<typename SettingType>
using RemoteSaveType = RestRemoteSetting<SettingType>;

using DeviceSettingsType = DeviceSettings<max_num_devices, 
                                        Ds18x20Driver, 
                                        Ads111xDriver,
                                        Pcf8575Driver,
                                        PinDriver, 
                                        LoadCellDriver,
                                        #ifdef ENABLE_DAC_DRIVER
                                        DacDriver,
                                        #endif
                                        ScheduleDriver, 
                                        DRV8825Driver,
                                        StepperDosingPumpDriver,
                                        SwitchDriver,
                                        PhProbeDriver,
                                        PicoDeviceDriver,
                                        DhtXXDriver,
                                        Bme280Driver>;
using StatCollectionType = StatCollection<max_stat_size>;

using SettingType = Settings<max_setting_size>;

// TODO: add settings
using GlobalStoreType = Store<
    SingleTypeStore<DeviceSettingsType,
    LocalSaveType<DeviceSettingsType::TrivialRepresentationType,
    ConstexprPath("devices.bin")>>
    // , SingleTypeStore<SettingType, LocalSaveType<SettingType::TrivialRepresentationType, ConstexprPath("settings.bin")>>
    >;
extern std::unique_ptr<GlobalStoreType, SPIRAMDeleter<GlobalStoreType>> global_store;