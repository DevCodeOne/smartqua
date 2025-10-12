#pragma once

#include "ds18x20.h"

#include <array>
#include <string_view>
#include <optional>
#include <thread>
#include <memory>
#include <shared_mutex>

#include "drivers/device_types.h"
#include "drivers/driver_interface.h"
#include "drivers/device_resource.h"
#include "utils/container/sample_container.h"
#include "utils/container/fixed_size_optional_array.h"
#include "build_config.h"

struct Ds18x20DriverData final {
    gpio_num_t gpio;
    ds18x20_addr_t addr;
};

class Ds18x20Driver;

template<>
struct DriverInfo<Ds18x20Driver> {
    using DeviceConfig = Ds18x20DriverData;

    static constexpr char Name[] = "ds18x20_driver";
};


class Ds18x20Driver final {
    public:
        Ds18x20Driver(const Ds18x20Driver &other) = delete;
        Ds18x20Driver(Ds18x20Driver &&other) noexcept;
        ~Ds18x20Driver();

        Ds18x20Driver &operator=(const Ds18x20Driver &other) = delete;
        Ds18x20Driver &operator=(Ds18x20Driver &&other) noexcept;

        static std::optional<Ds18x20Driver> create_driver(const std::string_view& input, DeviceConfig &deviceConfOut);
        static std::optional<Ds18x20Driver> create_driver(const DeviceConfig *config);

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;

        DeviceState oneIteration();
        bool reinit();
    private:
        Ds18x20Driver(const DeviceConfig*conf, std::shared_ptr<GpioResource> pin);

        static bool addAddress(ds18x20_addr_t address);
        static bool removeAddress(ds18x20_addr_t address);

        const DeviceConfig *mConf;

        std::shared_ptr<GpioResource> mPin;
        SampleContainer<float> mTemperatureReadings;

        static inline FixedSizeOptionalArray<ds18x20_addr_t, max_num_devices> _deviceAddresses;
        static inline std::shared_mutex _instanceMutex;
};