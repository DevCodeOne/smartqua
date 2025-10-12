#pragma once

#include "bmp280.h"

#include "drivers/driver_interface.h"
#include "drivers/device_resource.h"
#include "utils/container/lookup_table.h"
#include "utils/container/sample_container.h"

enum struct Bme280Address : std::decay_t<decltype(BMP280_I2C_ADDRESS_0)> {
    Zero = BMP280_I2C_ADDRESS_0,
    One = BMP280_I2C_ADDRESS_1,
    Invalid = 255,
};

template<>
struct read_from_json<Bme280Address> {
    static void read(const char *str, int len, Bme280Address &address) {
        std::string_view input(str, len);

        address = Bme280Address::Invalid;

        if (input == "0") {
            address = Bme280Address::Zero;
        } else if (input == "1") {
            address = Bme280Address::One;
        }
    }
};


struct Bme280DeviceConfig {
    gpio_num_t sdaPin;
    gpio_num_t sclPin;
    Bme280Address address;
    SampleContainerSettings<float> humidityContainerSettings;
    SampleContainerSettings<float> temperatureContainerSettings;
    SampleContainerSettings<float> pressureContainerSettings;
};

class Bme280Driver;

template<>
struct DriverInfo<Bme280Driver> {
    using DeviceConfig = Bme280DeviceConfig;

    static constexpr char Name[] = "Bme280Driver";
};

class Bme280Driver {
public:
    ~Bme280Driver();
    Bme280Driver(Bme280Driver &&other) noexcept;
    Bme280Driver &operator=(Bme280Driver &&other) noexcept;

    Bme280Driver(const Bme280Driver &other) = delete;
    Bme280Driver &operator=(const Bme280Driver &other) = delete;

    static std::optional<Bme280Driver> create_driver(const std::string_view &input, DeviceConfig &deviceConfigOut);
    static std::optional<Bme280Driver> create_driver(const DeviceConfig *data);

    DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
    DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;

    DeviceState oneIteration();
    bool reinit();
private:
    using AddressTracker = ResourceLookupTable<ThreadSafety::Safe,
                                        Bme280Address::Zero,
                                        Bme280Address::One>;
    using SingleAddressTracker = AddressTracker::FlagTrackerType;

    static std::optional<Bme280Driver> setupDevice(const DeviceConfig *deviceConf, SingleAddressTracker tracker,
                                                   std::shared_ptr<I2cResource> i2cResource);

    static bool initDevice(bmp280_t &device, const Bme280DeviceConfig* config, const I2cResource* i2cResource);

    Bme280Driver(const DeviceConfig *config,
                 SingleAddressTracker tracker,
                 std::shared_ptr<I2cResource> i2cResource,
                 bmp280_t device);

    const DeviceConfig *mConf;
    SingleAddressTracker mTracker;
    std::shared_ptr<I2cResource> mI2cResource;
    std::optional<bmp280_t> mDevice;

    SampleContainer<float> mHumidity;
    SampleContainer<float> mTemperature;
    SampleContainer<float> mPressure;

    static inline AddressTracker addressLookupTable;
};
