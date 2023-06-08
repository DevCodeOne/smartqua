#include "stepper_dosing_pump_driver.h"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <thread>

#include "driver/rmt_encoder.h"
#include "drivers/device_types.h"
#include "hal/gpio_types.h"
#include "drivers/device_resource.h"
#include "drivers/rmt_stepper_driver.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_types.h"
#include "frozen.h"

std::optional<StepperDosingPumpDriver> StepperDosingPumpDriver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
    StepperDosingConfig newConf;

    int stepPin = newConf.stepGPIONum;
    int enPin = newConf.enGPIONum;

    json_scanf(input.data(), input.size(), "{ step_gpio : %d,  en_gpio : %d }", &stepPin, &enPin);
      
    bool assignResult = true;

    assignResult &= check_assign(newConf.stepGPIONum, stepPin);
    assignResult &= check_assign(newConf.enGPIONum, enPin);

    if (!assignResult) {
        Logger::log(LogLevel::Error, "Some value(s) were out of range");
    }

    std::memcpy(reinterpret_cast<StepperDosingConfig *>(device_conf_out.device_config.data()), &newConf, sizeof(StepperDosingConfig));

    return create_driver(&device_conf_out);
}

std::optional<StepperDosingPumpDriver> StepperDosingPumpDriver::create_driver(const DeviceConfig*config) {
    StepperDosingConfig *const stepperConfig = reinterpret_cast<StepperDosingConfig *>(config->device_config.data());

    auto stepGPIO = device_resource::get_gpio_resource(static_cast<gpio_num_t>(stepperConfig->stepGPIONum), gpio_purpose::gpio);

    if (stepGPIO == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't get gpio resource for step pin");
        return std::nullopt;
    }

    auto enGPIO = device_resource::get_gpio_resource(static_cast<gpio_num_t>(stepperConfig->enGPIONum), gpio_purpose::gpio);

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
        .gpio_num = static_cast<uint8_t>(stepGPIO->gpio_num()),
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

    return StepperDosingPumpDriver{config, std::move(stepGPIO), RmtHandles{ .channel_handle = motor_channel}};
}

// TODO: safe value of mStepsLeft in seperate remotevariable
DeviceOperationResult StepperDosingPumpDriver::write_value(std::string_view what, const device_values &value) {
    const StepperDosingConfig *const stepperConfig = reinterpret_cast<const StepperDosingConfig *>(mConf->device_config.data());
    if (!value.milliliter()) {
        Logger::log(LogLevel::Error, "A dosing pump only supports values in ml");
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Info, "Dosing %d ml", (int) *value.milliliter());
    mStepsLeft.fetch_add(*value.milliliter() * (stepperConfig->stepsTimesTenPerMl / 10));

    return DeviceOperationResult::ok;
}

DeviceOperationResult StepperDosingPumpDriver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    StepperDosingConfig *const stepperConfig = reinterpret_cast<StepperDosingConfig *>(conf->device_config.data());
    if (action == "manual") {
        int steps = 0;
        float milliliter = 0;
        json_scanf(json.data(), json.size(), "{ steps : %d, ml : %f}", &steps, &milliliter);

        if ( milliliter != 0) {
            steps = static_cast<int>(milliliter * (stepperConfig->stepsTimesTenPerMl / 10.0f));
        }

        mStepsLeft.fetch_add(steps);
    } else if (action == "callibrate") {
        int steps = 200;
        float milliliter = 1.0f;
        json_scanf(json.data(), json.size(), "{ steps : %d, ml : %f }", &steps, &milliliter);

        Logger::log(LogLevel::Info, "%.*s, %d, %d", (int) json.size(), json.data(), steps, milliliter);
        stepperConfig->stepsTimesTenPerMl = static_cast<uint16_t>((steps * 10) / milliliter);
    }

    return DeviceOperationResult::ok;
}

void StepperDosingPumpDriver::updatePumpThread(std::stop_token token, StepperDosingPumpDriver *instance) {
    UniformStepperMovement mov{.resolution = 1'000'000 };
    rmt_encoder_handle_t encoder = nullptr;

    if(auto result = createNewRmtUniformEncoder(mov, &encoder); result != ESP_OK) {
        Logger::log(LogLevel::Error, "Couldn't create Rmt Uniform Encoder");
        return;
    }

    rmt_transmit_config_t tx_config{
        .loop_count = 0
    };

    const StepperDosingConfig *const stepperConfig = reinterpret_cast<const StepperDosingConfig *>(
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

StepperDosingPumpDriver::~StepperDosingPumpDriver() {
    Logger::log(LogLevel::Info, "Deleting instance of stepperdosingpumpdriver");
    if (mPumpThread.joinable()) {
        mPumpThread.request_stop();
        mPumpThread.join();
    }
}

StepperDosingPumpDriver::StepperDosingPumpDriver(const DeviceConfig*conf, std::shared_ptr<gpio_resource> stepGPIO, const RmtHandles &rmtHandle) : 
    mRmtHandles(rmtHandle),
    mConf(conf),
    mStepGPIO(stepGPIO),
    mStepsLeft(0)
{ 
    mPumpThread = std::jthread(&StepperDosingPumpDriver::updatePumpThread, this);
}


StepperDosingPumpDriver::StepperDosingPumpDriver(StepperDosingPumpDriver &&other) :
    mRmtHandles(std::move(other.mRmtHandles)),
    mConf(std::move(other.mConf)),
    mStepGPIO(std::move(other.mStepGPIO)),
    mStepsLeft(0)
{
    other.mPumpThread.request_stop();
    if (other.mPumpThread.joinable()) {
        other.mPumpThread.join();
    }
    
    mPumpThread = std::jthread(&StepperDosingPumpDriver::updatePumpThread, this);
}

StepperDosingPumpDriver &StepperDosingPumpDriver::operator=(StepperDosingPumpDriver &&other)
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

    mPumpThread = std::jthread(&StepperDosingPumpDriver::updatePumpThread, this);

    return *this; 
}