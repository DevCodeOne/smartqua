#include "bme280_driver.h"

#include "bmp280.h"

std::optional<Bme280Driver> Bme280Driver::create_driver(const std::string_view &input, DeviceConfig &deviceConfigOut) {
    unsigned int sdaPin = static_cast<uint8_t>(sdaDefaultPin);
    unsigned int sclPin = static_cast<uint8_t>(sclDefaultPin);


    Bme280DeviceConfig data{
        .sdaPin = static_cast<gpio_num_t>(sdaPin),
        .sclPin = static_cast<gpio_num_t>(sclPin),
        .address = Bme280Address::Zero,

        .humidityContainerSettings{30.0f},
        .temperatureContainerSettings{1.0f},
        .pressureContainerSettings{100.0f}
    };


    Logger::log(LogLevel::Info, "%.*s", input.size(), input.data());
    json_scanf(input.data(), input.size(),
               "{ address : %M, sclPin : %u, sdaPin : %u, humidity_container : %M, temperature_container : %M, pressure_container : %M }",
               json_scanf_single<decltype(data.address)>, &data.address, &sclPin, &sdaPin,
               json_scanf_single<decltype(data.humidityContainerSettings)>, &data.humidityContainerSettings,
               json_scanf_single<decltype(data.temperatureContainerSettings)>, &data.temperatureContainerSettings,
               json_scanf_single<decltype(data.pressureContainerSettings)>, &data.pressureContainerSettings);

    if (data.address == Bme280Address::Invalid) {
        Logger::log(LogLevel::Warning, "Invalid address for Bme280Driver : %x", static_cast<int>(data.address));
    }

    data.sclPin = static_cast<gpio_num_t>(sclPin);
    data.sdaPin = static_cast<gpio_num_t>(sdaPin);

    std::memcpy(deviceConfigOut.device_config.data(), &data, sizeof(Bme280DeviceConfig));
    deviceConfigOut.device_driver_name = DriverInfo<Bme280Driver>::Name;

    return create_driver(&deviceConfigOut);
}

std::optional<Bme280Driver> Bme280Driver::create_driver(const DeviceConfig *conf) {
    auto driverData = conf->accessConfig<Bme280DeviceConfig>();

    auto i2cResource = DeviceResource::get_i2c_port(i2c_port_t::I2C_NUM_0, I2C_MODE_MASTER, driverData->sdaPin, driverData->sclPin);

    if (i2cResource == nullptr)
    {
        Logger::log(LogLevel::Warning,
                    "I2C Port couldn't be acquired");
        return {std::nullopt};
    }

    auto flagTracker = addressLookupTable.setIfValue(driverData->address, true, false);

    if (!flagTracker) {
        Logger::log(LogLevel::Warning, "Couldn't get address %x for bme280", static_cast<int>(driverData->address));
        return { std::nullopt };
    }

    Logger::log(LogLevel::Debug, "Using address %u for bme280", static_cast<int>(driverData->address));

    return setupDevice(conf, std::move(flagTracker), std::move(i2cResource));
}

std::optional<Bme280Driver> Bme280Driver::setupDevice(const DeviceConfig *deviceConf,
                                                      SingleAddressTracker tracker,
                                                      std::shared_ptr<I2cResource> i2cResource) {
    auto driverData = deviceConf->accessConfig<Bme280DeviceConfig>();
    bmp280_t device{};
    if (!initDevice(device, driverData, i2cResource.get()))
    {
        return {};
    }

    Logger::log(LogLevel::Debug, "Successfully created bme280 device with address %u", driverData->address);

    return { Bme280Driver(deviceConf, std::move(tracker), std::move(i2cResource), device) };
}

bool Bme280Driver::initDevice(bmp280_t& device, const Bme280DeviceConfig* config, const I2cResource* i2cResource)
{
    auto result = bmp280_init_desc(&device, static_cast<uint8_t>(config->address),
                                   I2C_NUM_0,
                                   i2cResource->sda_pin(), i2cResource->scl_pin());
    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't create bme280 device, removing address from use list");
        return false;
    }

    bmp280_params_t params{};
    result = bmp280_init_default_params(&params);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't create bmp280 params");
        return false;
    }

    Logger::log(LogLevel::Debug, "Attempting to create bme280 device with address %u", config->address);

    device.i2c_dev.cfg.clk_flags = 0;
    device.i2c_dev.cfg.master.clk_speed = 100'000;
    device.i2c_dev.cfg.scl_pullup_en = true;
    device.i2c_dev.cfg.sda_pullup_en = true;
    result = bmp280_init(&device, &params);
    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't init bmp280 device");
        return false;
    }

    return true;
}

DeviceOperationResult Bme280Driver::read_value(std::string_view what, DeviceValues &value) const {
    if (what == "temperature") {
        value.setToUnit(DeviceValueUnit::temperature, mTemperature.average());
    } else if (what == "humidity") {
        value.setToUnit(DeviceValueUnit::humidity, mHumidity.average());
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

DeviceState Bme280Driver::oneIteration() {
    if (!mDevice.has_value()) {
        return DeviceState::ReadError;
    }

    float temperature = 0;
    float pressure = 0;
    float humidity = 0;
    auto result = bmp280_read_float(&mDevice.value(), &temperature, &pressure, &humidity);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't get measurements");
        return DeviceState::ReadError;
    }

    Logger::log(LogLevel::Info, "Got temperature : %f, humidity : %f, pressure : %f ",
                temperature,
                humidity,
                pressure);

    const std::array anySampleIssue{
        std::pair{"humidity", mHumidity.putSample(humidity)},
        std::pair{"temperature", mTemperature.putSample(temperature)},
        std::pair{"pressure", mPressure.putSample(pressure)}
    };

    bool thereWasAnIssue = false;
    for (const auto& [name, successful] : anySampleIssue)
    {
        if (!successful)
        {
            Logger::log(LogLevel::Warning, "There was an issue with the data provided by the sensor: %s",
                        name);
            thereWasAnIssue = true;
        }
    }

    if (thereWasAnIssue)
    {
        return DeviceState::SampleIssue;
    }

    return DeviceState::Ok;
}

bool Bme280Driver::reinit()
{
    auto driverConf = mConf->accessConfig<Bme280DeviceConfig>();
    return initDevice(mDevice.value(), driverConf, mI2cResource.get());
}

Bme280Driver::Bme280Driver(const DeviceConfig *config,
                           SingleAddressTracker tracker,
                           std::shared_ptr<I2cResource> i2cResource,
                           bmp280_t device)
    : mConf(config),
    mTracker(std::move(tracker))
    , mI2cResource(std::move(i2cResource))
    , mDevice(device)
    , mHumidity(config->accessConfig<Bme280DeviceConfig>()->humidityContainerSettings)
    , mTemperature(config->accessConfig<Bme280DeviceConfig>()->temperatureContainerSettings)
    , mPressure(config->accessConfig<Bme280DeviceConfig>()->pressureContainerSettings)
{
}

Bme280Driver::~Bme280Driver() {
    if (mDevice) {
        bmp280_free_desc(&mDevice.value());
    }
}

Bme280Driver::Bme280Driver(Bme280Driver&& other) noexcept
    : mTracker(std::move(other.mTracker))
      , mI2cResource(std::move(other.mI2cResource))
      ,
      mDevice(other
          .
          mDevice
      )
{
    other.mDevice = std::nullopt;
}

Bme280Driver & Bme280Driver::operator=(Bme280Driver &&other) noexcept
{
    using std::swap;

    swap(other.mTracker, mTracker);
    swap(other.mI2cResource, mI2cResource);

    other.mDevice = std::nullopt;

    return *this;
}
