#include "drivers/dhtxx_driver.h"

#include "dht.h"

std::optional<DhtXXDriver> DhtXXDriver::create_driver(const DeviceConfig *config) {
    auto driver_data = config->accessConfig<DhtXXDriverData>();
    auto pin = DeviceResource::get_gpio_resource(driver_data->gpio, GpioPurpose::bus);

    if (!pin) {
        Logger::log(LogLevel::Warning, "GPIO pin couldn't be reserved");
        return std::nullopt;
    }

    return DhtXXDriver(config, pin);
}

DeviceOperationResult DhtXXDriver::write_value(std::string_view what, const DeviceValues &value) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DhtXXDriver::read_value(std::string_view what, DeviceValues &value) const {
    if (what == "temp" || what == "temperature") {
        value.temperature(mTemperatureReadings.average());
        return DeviceOperationResult::ok;
    }
    if (what == "humidity") {
        value.humidity(mHumidityReadings.average());
        return DeviceOperationResult::ok;
    }

    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DhtXXDriver::get_info(char *output, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

DeviceOperationResult DhtXXDriver::call_device_action(DeviceConfig *config, const std::string_view &action,
    const std::string_view &json) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DhtXXDriver::update_runtime_data() {
    return DeviceOperationResult::not_supported;
}

DhtXXDriver::DhtXXDriver(const DeviceConfig *config, std::shared_ptr<GpioResource> pin) : mConf(config), mPin(std::move(pin)) {
    mReadThread = std::jthread(&DhtXXDriver::updateDataThread, this);
}

void DhtXXDriver::updateDataThread(std::stop_token token, DhtXXDriver *instance) {
    using namespace std::chrono_literals;
    const auto *config  = instance->mConf->accessConfig<DhtXXDriverData>();

    while(!token.stop_requested()) {
        const auto beforeReading = std::chrono::steady_clock::now();
        float temperature = 0;
        float humidity = 0;
        const auto error = dht_read_float_data(static_cast<dht_sensor_type_t>(config->type),
                                         config->gpio,
                                         &humidity,
                                         &temperature);

        if (error != ESP_OK) {
            Logger::log(LogLevel::Error, "Failed to read temperature and/or humidity");
        }

        if (error == ESP_OK) {
            instance->mTemperatureReadings.put_sample(temperature);
            instance->mHumidityReadings.put_sample(humidity);
        }

        const auto duration = std::chrono::steady_clock::now() - beforeReading;
        std::this_thread::sleep_for(duration < 5s ? 5s - duration : 500ms);
    }
}

DhtXXDriver::DhtXXDriver(DhtXXDriver &&other) noexcept : mConf(other.mConf), mPin(std::move(other.mPin)) {
    other.mReadThread.request_stop();
    if (other.mReadThread.joinable()) {
        other.mReadThread.join();
    }

    other.mConf = nullptr;
    other.mPin = nullptr;

    mReadThread = std::jthread(&DhtXXDriver::updateDataThread, this);
}

DhtXXDriver::~DhtXXDriver() {
    if (mConf == nullptr) {
        return;
    }

    Logger::log(LogLevel::Info, "Deleting instance of DhtXXDriver");
    if (mReadThread.joinable()) {
        mReadThread.request_stop();
        mReadThread.join();
    }
}

DhtXXDriver &DhtXXDriver::operator=(DhtXXDriver &&other) noexcept {
    using std::swap;

    Logger::log(LogLevel::Info, "Waiting for other thread to join move op");
    other.mReadThread.request_stop();
    if (other.mReadThread.joinable()) {
        other.mReadThread.join();
    }
    Logger::log(LogLevel::Info, "Done");

    swap(mConf, other.mConf);
    swap(mPin, other.mPin);

    mReadThread = std::jthread(&DhtXXDriver::updateDataThread, this);

    return *this;
}

std::optional<DhtXXDriver> DhtXXDriver::create_driver(const std::string_view input, DeviceConfig &device_conf_out) {
    int gpio_num = -1;
    DhtXXDeviceType type;
    json_scanf(input.data(), input.size(), R"({ gpio_num : %d, type : %M })",
               &gpio_num, json_scanf_single<decltype(type)>, &type);
    Logger::log(LogLevel::Info, "gpio_num : %d", gpio_num);

    if (gpio_num == -1) {
        Logger::log(LogLevel::Warning, "Invalid gpio num : %d", gpio_num);
        return std::nullopt;
    }

    auto pin = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(gpio_num), GpioPurpose::bus);

    if (!pin) {
        Logger::log(LogLevel::Warning, "GPIO pin couldn't be reserved");
        return std::nullopt;
    }

    DhtXXDriverData data { .gpio = static_cast<gpio_num_t>(gpio_num), .type = DhtXXDeviceType::Dht21 };
    std::memcpy(device_conf_out.device_config.data(), &data, sizeof(DhtXXDriverData));
    device_conf_out.device_driver_name =  DhtXXDriver::name;

    return DhtXXDriver(&device_conf_out, pin);
}
