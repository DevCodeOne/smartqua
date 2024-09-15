#include "bme280_driver.h"

#include "bmp280.h"

std::optional<Bme280Driver> Bme280Driver::create_driver(const std::string_view &input, DeviceConfig &deviceConfigOut) {
    unsigned int sdaPin = static_cast<uint8_t>(sdaDefaultPin);
    unsigned int sclPin = static_cast<uint8_t>(sclDefaultPin);
    Bme280Address address = Bme280Address::Invalid;

    Logger::log(LogLevel::Info, "%.*s", input.size(), input.data());
    json_scanf(input.data(), input.size(), "{ address : %M, sclPin : %u, sdaPin : %u }",
               json_scanf_single<decltype(address)>, &address, &sclPin, &sdaPin);

    if (address == Bme280Address::Invalid) {
        Logger::log(LogLevel::Warning, "Invalid address for Bme280Driver : %x", static_cast<int>(address));
    }

    Bme280DeviceConfig data{
        .sdaPin = static_cast<gpio_num_t>(sdaPin),
        .sclPin = static_cast<gpio_num_t>(sclPin),
        .address = address,
    };

    std::memcpy(deviceConfigOut.device_config.data(), &data, sizeof(Bme280DeviceConfig));
    deviceConfigOut.device_driver_name = DriverInfo<Bme280Driver>::Name;

    return create_driver(&deviceConfigOut);
}

std::optional<Bme280Driver> Bme280Driver::create_driver(const DeviceConfig *conf) {
    auto driverData = reinterpret_cast<const Bme280DeviceConfig *>(conf->device_config.data());

    auto sdaPin = DeviceResource::get_gpio_resource(driverData->sdaPin, GpioPurpose::bus);
    auto sclPin = DeviceResource::get_gpio_resource(driverData->sclPin, GpioPurpose::bus);

    if (!sdaPin || !sclPin) {
        Logger::log(LogLevel::Warning,
            "I2C Port cannot be used, since one or more pins are already reserved for a different purpose");
        return { std::nullopt };
    }

    auto flagTracker = addressLookupTable.setIfValue(driverData->address, true, false);

    if (!flagTracker) {
        Logger::log(LogLevel::Warning, "Couldn't get address %x for bme280", static_cast<int>(driverData->address));
        return { std::nullopt };
    }



    return setupDevice(conf, std::move(flagTracker), std::move(sdaPin), std::move(sclPin));
}

std::optional<Bme280Driver> Bme280Driver::setupDevice(const DeviceConfig *deviceConf,
                                                      SingleAddressTracker tracker,
                                                      std::shared_ptr<GpioResource> sdaPin,
                                                      std::shared_ptr<GpioResource> sclPin) {
    auto driverData = reinterpret_cast<const Bme280DeviceConfig *>(deviceConf->device_config.data());
    bmp280_t device{};
    std::memset(&device, 0, sizeof(bmp280_t));
    device.i2c_dev.cfg.clk_flags = 0;
    auto result = bmp280_init_desc(&device, static_cast<uint8_t>(driverData->address),
                                   I2C_NUM_0,
                                   sdaPin->gpio_num(), sclPin->gpio_num());
    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't create bme280 device, removing address from use list");
    }

    bmp280_params_t params{};
    result = bmp280_init_default_params(&params);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't create bmp280 params");
        return { std::nullopt };
    }

    result = bmp280_init(&device, &params);
    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't init bmp280 device");
        return { std::nullopt };
    }

    return { Bme280Driver(deviceConf, std::move(tracker), std::move(sdaPin), std::move(sclPin), device) };
}

DeviceOperationResult Bme280Driver::read_value(std::string_view what, DeviceValues &value) const {
    if (what == "temperature") {
        value.temperature(mTemperature.average());
    } else if (what == "humidity") {
        value.humidity(mHumidity.average());
    } else if (what == "pressure") {
        // TODO: implement
        return DeviceOperationResult::not_supported;
    }

    return DeviceOperationResult::ok;
}

DeviceOperationResult Bme280Driver::get_info(char *output, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

void Bme280Driver::oneIteration(Bme280Driver *instance) {
    if (!instance->mDevice.has_value()) {
        return;
    }

    float temperature = 0;
    float pressure = 0;
    float humidity = 0;
    auto result = bmp280_read_float(&instance->mDevice.value(), &temperature, &pressure, &humidity);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't get measurements");
        return;
    }

    Logger::log(LogLevel::Info, "Got temperature : %f, humidity : %f, pressure : %f ",
                temperature,
                humidity,
                pressure);

    instance->mHumidity.put_sample(humidity);
    instance->mTemperature.put_sample(temperature);
    instance->mPressure.put_sample(pressure);
}

Bme280Driver::Bme280Driver(const DeviceConfig *config,
                           SingleAddressTracker tracker,
                           std::shared_ptr<GpioResource> sdaPin,
                           std::shared_ptr<GpioResource> sclPin,
                           bmp280_t device)
    : DriverInterface(config)
      , mTracker(std::move(tracker))
      , mSdaPin(std::move(sdaPin))
      , mSclPin(std::move(sclPin))
      , mDevice(device)
      , mHumidity()
      , mTemperature()
      , mPressure() {
}

Bme280Driver::~Bme280Driver() {
    if (mDevice) {
        bmp280_free_desc(&mDevice.value());
    }
}

Bme280Driver::Bme280Driver(Bme280Driver &&other)
    : DriverInterface(std::move(other))
    , mTracker(std::move(other.mTracker))
    , mSdaPin(std::move(other.mSdaPin))
    , mSclPin(std::move(other.mSclPin))
    , mDevice(other.mDevice) {
    other.mDevice = std::nullopt;
}

Bme280Driver & Bme280Driver::operator=(Bme280Driver &&other) {
    using std::swap;

    swap(other.mTracker, mTracker);
    swap(other.mSdaPin, mSdaPin);
    swap(other.mSclPin, mSclPin);

    other.mDevice = std::nullopt;

    return *this;
}
