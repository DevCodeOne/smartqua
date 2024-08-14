#include "drv8825_driver.h"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <thread>

#include "driver/rmt_encoder.h"
#include "drivers/device_types.h"
#include "hal/gpio_types.h"
#include "drivers/device_resource.h"
#include "drivers/rmt_stepper_driver.h"
#include "utils/check_assign.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_types.h"
#include "frozen.h"

std::optional<DRV8825Driver> DRV8825Driver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
    DRV8825DriverConfig newConf;

    int stepPin = newConf.stepGPIONum;
    int enPin = newConf.enGPIONum;

    json_scanf(input.data(), input.size(), "{ step_gpio : %d,  en_gpio : %d }", &stepPin, &enPin);
      
    bool assignResult = true;

    assignResult &= checkAssign(newConf.stepGPIONum, stepPin);
    assignResult &= checkAssign(newConf.enGPIONum, enPin);

    if (!assignResult) {
        Logger::log(LogLevel::Error, "Some value(s) were out of range");
    }

    std::memcpy(reinterpret_cast<DRV8825DriverConfig *>(device_conf_out.device_config.data()), &newConf, sizeof(DRV8825DriverConfig));

    return create_driver(&device_conf_out);
}

std::optional<DRV8825Driver> DRV8825Driver::create_driver(const DeviceConfig*config) {
    DRV8825DriverConfig *const stepperConfig = reinterpret_cast<DRV8825DriverConfig *>(config->device_config.data());

    auto stepGPIO = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(stepperConfig->stepGPIONum), GpioPurpose::gpio);

    if (stepGPIO == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't get gpio resource for step pin");
        return std::nullopt;
    }

    auto enGPIO = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(stepperConfig->enGPIONum), GpioPurpose::gpio);

    gpio_config_t stepGPIOConf{
        .pin_bit_mask = 1ULL << static_cast<uint8_t>(stepGPIO->gpio_num()) | 1ULL << static_cast<uint8_t>(enGPIO->gpio_num()),
        .mode = GPIO_MODE_OUTPUT, 
        .intr_type = GPIO_INTR_DISABLE,
    };


    if (auto result = gpio_config(&stepGPIOConf); result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't initialize gpio with config");
        return std::nullopt;
    }

    rmt_channel_handle_t motor_channel = nullptr;
    rmt_tx_channel_config_t tx_channel_conf{
        .gpio_num = static_cast<gpio_num_t>(stepGPIO->gpio_num()),
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1'000'000, // 1 MHz
        .mem_block_symbols = 64,
        .trans_queue_depth = 2 // number of transactions
    };

    if (auto result = rmt_new_tx_channel(&tx_channel_conf, &motor_channel); result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't create tx channel");
        return std::nullopt;
    }

    if (auto result = rmt_enable(motor_channel); result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't enable rmt_channel");
        return std::nullopt;
    }

    return DRV8825Driver{config, std::move(stepGPIO), RmtHandles{ .channel_handle = motor_channel}};
}

// TODO: safe value of mStepsLeft in seperate remotevariable
DeviceOperationResult DRV8825Driver::write_value(std::string_view what, const DeviceValues &value) {
    const DRV8825DriverConfig *const stepperConfig = reinterpret_cast<const DRV8825DriverConfig *>(mConf->device_config.data());
    if (!value.generic_unsigned_integral()) {
        Logger::log(LogLevel::Error, "A dosing pump only supports steps");
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Info, "Doing %u steps", (unsigned int) *value.generic_unsigned_integral());
    mStepsLeft.fetch_add(*value.generic_unsigned_integral());

    return DeviceOperationResult::ok;
}

DeviceOperationResult DRV8825Driver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::ok;
}

void DRV8825Driver::updatePumpThread(std::stop_token token, DRV8825Driver *instance) {
    UniformStepperMovement mov{.resolution = 1'000'000 };
    rmt_encoder_handle_t encoder = nullptr;

    if(auto result = createNewRmtUniformEncoder(mov, &encoder); result != ESP_OK) {
        Logger::log(LogLevel::Error, "Couldn't create Rmt Uniform Encoder");
        return;
    }

    rmt_transmit_config_t tx_config{
        .loop_count = 0
    };

    const DRV8825DriverConfig *const stepperConfig = reinterpret_cast<const DRV8825DriverConfig *>(
            instance->mConf->device_config.data());

    Logger::log(LogLevel::Info, "Starting Stepper dosing thread");
    auto result = gpio_set_level(static_cast<gpio_num_t>(stepperConfig->enGPIONum), 1);
    Logger::log(LogLevel::Info, "Result setting enable to 1 %d", result);

    while (!token.stop_requested()) {
        const auto currentStepsLeft = instance->mStepsLeft.exchange(0);

        if (currentStepsLeft > 0) {

            gpio_set_level(static_cast<gpio_num_t>(stepperConfig->enGPIONum), 0);

            uint32_t speed_hz = 100;
            if (auto result = rmt_transmit(instance->mRmtHandles.channel_handle, encoder, &speed_hz, sizeof(speed_hz), &tx_config); result != ESP_OK) {
                Logger::log(LogLevel::Error, "Couldn't transmit data");
                // return DeviceOperationResult::failure;
            }

            if (auto result = rmt_tx_wait_all_done(instance->mRmtHandles.channel_handle, 200); result != ESP_OK) {
                Logger::log(LogLevel::Error, "Couldn't wait for transmit of data");
                // return DeviceOperationResult::failure;
            }

            gpio_set_level(static_cast<gpio_num_t>(stepperConfig->enGPIONum), 1);

            instance->mStepsLeft.fetch_add(currentStepsLeft - 1);
        } else {
            // Logger::log(LogLevel::Info, "Waiting for different value");
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(100ms);
        }

    }
    Logger::log(LogLevel::Info, "Exiting pump thread");
}

DRV8825Driver::~DRV8825Driver() {
    Logger::log(LogLevel::Info, "Deleting instance of stepperdosingpumpdriver");
    if (mPumpThread.joinable()) {
        mPumpThread.request_stop();
        mPumpThread.join();
    }
}

DRV8825Driver::DRV8825Driver(const DeviceConfig*conf, std::shared_ptr<GpioResource> stepGPIO, const RmtHandles &rmtHandle) :
    mRmtHandles(rmtHandle),
    mConf(conf),
    mStepGPIO(stepGPIO),
    mStepsLeft(0)
{ 
    mPumpThread = std::jthread(&DRV8825Driver::updatePumpThread, this);
}


DRV8825Driver::DRV8825Driver(DRV8825Driver &&other) :
    mRmtHandles(std::move(other.mRmtHandles)),
    mConf(std::move(other.mConf)),
    mStepGPIO(std::move(other.mStepGPIO)),
    mStepsLeft(0)
{
    other.mPumpThread.request_stop();
    if (other.mPumpThread.joinable()) {
        other.mPumpThread.join();
    }
    
    mPumpThread = std::jthread(&DRV8825Driver::updatePumpThread, this);
}

DRV8825Driver &DRV8825Driver::operator=(DRV8825Driver &&other)
{
    Logger::log(LogLevel::Info, "Waiting for other thread to join move op");
    other.mStepsLeft = 1;
    other.mPumpThread.request_stop();
    if (other.mPumpThread.joinable()) {
        other.mPumpThread.join();
    }
    Logger::log(LogLevel::Info, "Done");

    using std::swap;
    swap(mRmtHandles, other.mRmtHandles);
    swap(mConf, other.mConf);
    swap(mStepGPIO, other.mStepGPIO);
    Logger::log(LogLevel::Info, "Done");
    mStepsLeft = 0;

    mPumpThread = std::jthread(&DRV8825Driver::updatePumpThread, this);

    return *this; 
}