#pragma once 

#include <cstdint>
#include <memory>
#include <thread>
#include <string_view>

#include "build_config.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "build_config.h"

#include "driver/rmt_types.h"

struct DRV8825DriverConfig {
    uint8_t stepGPIONum = 14; 
    uint8_t enGPIONum = 17;
    uint16_t maxFrequency = 2000;
};

// TODO: direction
// TODO: add generic_signed_integral, where the sign signals the direction
class DRV8825Driver final {
    public:
        static inline constexpr char name[] = "drv8825_driver";

        DRV8825Driver(const DRV8825Driver &other) = delete;
        DRV8825Driver(DRV8825Driver &&other) noexcept;
        ~DRV8825Driver();

        DRV8825Driver &operator=(const DRV8825Driver &other) = delete;
        DRV8825Driver &operator=(DRV8825Driver &&other) noexcept;

        static std::optional<DRV8825Driver> create_driver(const std::string_view input, DeviceConfig&deviceConfOut);
        static std::optional<DRV8825Driver> create_driver(const DeviceConfig*config);

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::not_supported; }

    private:
        static void updatePumpThread(std::stop_token token, DRV8825Driver *instance);

        struct RmtHandles {
            rmt_channel_handle_t channel_handle;
        } mRmtHandles;

        DRV8825Driver(const DeviceConfig*conf, std::shared_ptr<GpioResource> stepGPIO, const RmtHandles &rmtHandle);

        const DeviceConfig *mConf;

        std::shared_ptr<GpioResource> mStepGPIO = nullptr;
        std::atomic_uint16_t mStepsLeft = 0;
        std::jthread mPumpThread;
};