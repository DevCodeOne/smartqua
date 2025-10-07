#pragma once

#include <optional>
#include <string_view>

#include "dht.h"

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "drivers/driver_interface.h"

#include "utils/container/sample_container.h"

enum struct DhtXXDeviceType : uint8_t {
    Dht21 = dht_sensor_type_t::DHT_TYPE_AM2301
};

template<>
struct read_from_json<DhtXXDeviceType> {
    static void read(const char *str, int len, DhtXXDeviceType &type) {
        std::string_view input(str, len);

        type = DhtXXDeviceType::Dht21;

        if (input == "DHT21" ||  input == "dht21") {
            type = DhtXXDeviceType::Dht21;
        }
    }
};

struct DhtXXDriverData final {
    gpio_num_t gpio;
    DhtXXDeviceType type;
};

class DhtXXDriver;

template<>
struct DriverInfo<DhtXXDriver> {
    using DeviceConfig = DhtXXDriverData;

    static constexpr char Name[] = "dhtxx_driver";
};


class DhtXXDriver final {
    public:

    DhtXXDriver(const DhtXXDriver&) = delete;
    DhtXXDriver(DhtXXDriver&&) noexcept;
    ~DhtXXDriver();

    DhtXXDriver& operator=(const DhtXXDriver&) = delete;
    DhtXXDriver& operator=(DhtXXDriver&&) noexcept;

    static std::optional<DhtXXDriver> create_driver(std::string_view input, DeviceConfig &deviceConfig);
    static std::optional<DhtXXDriver> create_driver(const DeviceConfig *deviceConfig);

    DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
    DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
    DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
    DeviceOperationResult call_device_action(DeviceConfig *config, const std::string_view &action, const std::string_view &json) const;
    DeviceOperationResult update_runtime_data();

    void oneIteration();

    private:
    DhtXXDriver(const DeviceConfig *config, std::shared_ptr<GpioResource> pin);

    const DeviceConfig *mConf;
    std::shared_ptr<GpioResource> mPin;
    SampleContainer<float> mTemperatureReadings{};
    SampleContainer<float> mHumidityReadings{};

    static inline std::array<std::shared_ptr<GpioResource>, max_num_devices> _Pins;
    static inline std::shared_mutex _instance_mutex;

};