#include "scale_driver.h"

#include <optional>
#include <string_view>
#include <cstring>

#include "drivers/device_resource.h"
#include "drivers/device_types.h"
#include "utils/check_assign.h"

#include "hx711.h"
#include "frozen.h"

LoadcellDriver::LoadcellDriver(const DeviceConfig*conf, 
    std::shared_ptr<gpio_resource> doutGPIO, 
    std::shared_ptr<gpio_resource> sckGPIO,
    hx711_t &&dev) : mConf(conf), mDoutGPIO(std::move(doutGPIO)), mSckGPIO(std::move(sckGPIO)), mDev(dev) {}

LoadcellDriver::LoadcellDriver(LoadcellDriver &&other) : 
    mConf(other.mConf),
    mDoutGPIO(std::move(other.mDoutGPIO)),
    mSckGPIO(std::move(other.mSckGPIO)),
    mDev(other.mDev) {}

LoadcellDriver &LoadcellDriver::operator=(LoadcellDriver &&other) { 
    std::swap(mDoutGPIO, other.mDoutGPIO);
    std::swap(mSckGPIO, other.mSckGPIO);
    std::swap(mDev, other.mDev);
    std::swap(mConf, other.mConf);
    return *this;
}

DeviceOperationResult LoadcellDriver::write_value(std::string_view what, const DeviceValues &value) const {
    return DeviceOperationResult::failure;
}

int32_t LoadcellDriver::convertToRealValue(int32_t rawValue, int32_t offset, int32_t scale) {
    return (rawValue + offset) * (scale / static_cast<float>(100'000));
}

DeviceOperationResult LoadcellDriver::read_value(std::string_view what, DeviceValues &out) const {
    const auto config = reinterpret_cast<LoadcellConfig *>(mConf->device_config.data());
    // TODO: prevent scale from being zero
    out.milligrams(convertToRealValue(m_values.average(), config->offset, config->scale));
    return DeviceOperationResult::ok;
}

DeviceOperationResult LoadcellDriver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    auto cellConfig = reinterpret_cast<LoadcellConfig *>(conf->device_config.data());
    // TODO: prevent this from happening when there are no samples
    if (action == "tare") {
        Logger::log(LogLevel::Info, "Taring loadcell");
        cellConfig->offset = -m_values.last();

        return DeviceOperationResult::ok;
    } else if (action == "callibrate") {
        if (json.data() == nullptr) {
            Logger::log(LogLevel::Info, "Input was empty");
            return DeviceOperationResult::failure;
        }

        Logger::log(LogLevel::Info, "Callibrating Loadcell json input : %s", json.data());
        int callibrationWeight = 1;
        json_scanf(json.data(), json.size(), "{ callib_weight_g : %d }", &callibrationWeight);

        if (callibrationWeight == 0) {
            // TODO: print error
            return DeviceOperationResult::failure;
        }

        cellConfig->scale = (callibrationWeight / static_cast<float>(m_values.last() + cellConfig->offset)) * 100'000;
        return DeviceOperationResult::ok;
    }

    return DeviceOperationResult::not_supported;
}

DeviceOperationResult LoadcellDriver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::failure;
}

std::optional<LoadcellDriver> LoadcellDriver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
    LoadcellConfig newConf;
    int offset = newConf.offset;
    int scale = newConf.scale;
    int sckGPIONum = newConf.sckGPIONum;
    int doutGPIONum = newConf.doutGPIONum;

    json_scanf(input.data(), input.size(), "{ offset : %d, scale : %d, sck_pin : %d, dout_pin : %d }", 
        &offset, &scale, &sckGPIONum, &doutGPIONum);

    bool assignResult = true;
    assignResult &= checkAssign(newConf.offset, offset);
    assignResult &= checkAssign(newConf.scale, scale);
    assignResult &= checkAssign(newConf.sckGPIONum, sckGPIONum);
    assignResult &= checkAssign(newConf.doutGPIONum, doutGPIONum);

    if (!assignResult) {
        Logger::log(LogLevel::Error, "Some value(s) were of out range");
        return std::nullopt;
    }

    std::memcpy(reinterpret_cast<LoadcellConfig *>(device_conf_out.device_config.data()), &newConf, sizeof(LoadcellConfig));
    return create_driver(&device_conf_out);
}

std::optional<LoadcellDriver> LoadcellDriver::create_driver(const DeviceConfig*config) {
    LoadcellConfig *const loadcellConf = reinterpret_cast<LoadcellConfig *>(config->device_config.data());

    auto doutGPIO = device_resource::get_gpio_resource(static_cast<gpio_num_t>(loadcellConf->doutGPIONum), gpio_purpose::gpio);

    if (doutGPIO == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't get gpio resource for dout");
        return std::nullopt;
    }

    auto sckGPIO = device_resource::get_gpio_resource(static_cast<gpio_num_t>(loadcellConf->sckGPIONum), gpio_purpose::gpio);

    if (sckGPIO == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't get gpio resource for sck");
        return std::nullopt;
    }

    hx711_t dev{
        .dout = doutGPIO->gpio_num(),
        .pd_sck = sckGPIO->gpio_num(),
        .gain = HX711_GAIN_A_64
    };

    const auto status = hx711_init(&dev);

    if (status != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't initialize scale");
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "create_driver added new scale device");

    return std::make_optional(LoadcellDriver{config, doutGPIO, sckGPIO, std::move(dev)});
}

DeviceOperationResult LoadcellDriver::update_runtime_data() {
    const auto config = reinterpret_cast<const LoadcellConfig *>(mConf->device_config.data());
    esp_err_t waitResult = hx711_wait(&mDev, 1000);
    bool isReady = false;

    if (waitResult != ESP_OK || hx711_is_ready(&mDev, &isReady) != ESP_OK || !isReady) {
        Logger::log(LogLevel::Info, "Scale wasn't ready");
        return DeviceOperationResult::failure;
    }

    int32_t value = 0;

    if (auto read_result = hx711_read_data(&mDev, &value); read_result != ESP_OK) {
        Logger::log(LogLevel::Error, "Failed to read from scale");
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Info, "Read raw value %ld, converted value %ld", 
        value, convertToRealValue(value, config->offset, config->scale));
    m_values.put_sample(value);

    return DeviceOperationResult::ok;
}