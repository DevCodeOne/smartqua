#pragma once 

#include <cstdint>
#include <memory>
#include <thread>
#include <string_view>

#include "build_config.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "smartqua_config.h"

#include "driver/rmt_types.h"

struct StepperDosingConfig {
    uint8_t stepGPIONum = 14; 
    uint8_t enGPIONum = 17;
    uint16_t stepsTimesTenPerMl = 200;
    uint16_t maxFrequency = 2000;
};

class StepperDosingPumpDriver final {
    public:
        static inline constexpr char name[] = "StepDosingPump";

        StepperDosingPumpDriver(const StepperDosingPumpDriver &other) = delete;
        StepperDosingPumpDriver(StepperDosingPumpDriver &&other);
        ~StepperDosingPumpDriver();

        StepperDosingPumpDriver &operator=(const StepperDosingPumpDriver &other) = delete;
        StepperDosingPumpDriver &operator=(StepperDosingPumpDriver &&other);

        static std::optional<StepperDosingPumpDriver> create_driver(const std::string_view input, device_config &device_conf_out);
        static std::optional<StepperDosingPumpDriver> create_driver(const device_config *config);

        DeviceOperationResult read_value(std::string_view what, device_values &value) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult write_value(const device_values &value);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::not_supported; }

    private:
        static void updatePumpThread(std::stop_token token, StepperDosingPumpDriver *instance);

        struct RmtHandles {
            rmt_channel_handle_t channel_handle;
        } mRmtHandles;

        StepperDosingPumpDriver(const device_config *conf, std::shared_ptr<gpio_resource> stepGPIO, const RmtHandles &rmtHandle);

        const device_config *mConf;

        std::shared_ptr<gpio_resource> mStepGPIO = nullptr;
        std::atomic_uint16_t mStepsLeft = 0;
        std::jthread mPumpThread;
};