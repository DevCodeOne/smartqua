#pragma once

#include "ds18x20.h"

#include <array>
#include <string_view>
#include <optional>
#include <thread>
#include <memory>
#include <shared_mutex>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "build_config.h"
#include "utils/sample_container.h"

struct ds18x20_driver_data final {
    gpio_num_t gpio;
    ds18x20_addr_t addr;
};

class Ds18x20Driver final {
    public:
        static inline constexpr char name[] = "ds18x20_driver";

        Ds18x20Driver(const Ds18x20Driver &other) = delete;
        Ds18x20Driver(Ds18x20Driver &&other);
        ~Ds18x20Driver();

        Ds18x20Driver &operator=(const Ds18x20Driver &other) = delete;
        Ds18x20Driver &operator=(Ds18x20Driver &&other);

        static std::optional<Ds18x20Driver> create_driver(const std::string_view input, DeviceConfig&device_conf_out);
        static std::optional<Ds18x20Driver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();
    private:
        Ds18x20Driver(const DeviceConfig*conf, std::shared_ptr<gpio_resource> pin);

        static void updateTempThread(std::stop_token token, Ds18x20Driver *instance);

        static bool add_address(ds18x20_addr_t address);
        static bool remove_address(ds18x20_addr_t address);

        const DeviceConfig*m_conf;

        std::shared_ptr<gpio_resource> m_pin;
        std::jthread mTemperatureThread;
        sample_container<float> mTemperatureReadings;

        static inline std::array<std::optional<ds18x20_addr_t>, max_num_devices> _device_addresses;
        static inline std::shared_mutex _instance_mutex;
};