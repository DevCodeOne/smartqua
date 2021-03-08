#pragma once

#include "ds18x20.h"

#include <array>
#include <string_view>
#include <optional>
#include <memory>
#include <shared_mutex>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "smartqua_config.h"

struct ds18x20_driver_data final {
    gpio_num_t gpio;
    ds18x20_addr_t addr;
};

class ds18x20_driver final {
    public:
        static inline constexpr char name[] = "ds18x20_driver";

        ~ds18x20_driver();

        static std::optional<ds18x20_driver> create_driver(const std::string_view input, device_config &device_conf_out);
        static std::optional<ds18x20_driver> create_driver(const device_config *config);

        device_operation_result write_value(const device_values &value);
        device_operation_result read_value(device_values &value) const;
        device_operation_result get_info(char *output, size_t output_buffer_len) const;
        device_operation_result write_device_options(const char *json_input, size_t input_len);
    private:
        ds18x20_driver(const device_config *conf, std::shared_ptr<gpio_resource> pin);

        static bool add_address(ds18x20_addr_t address);
        static bool remove_address(ds18x20_addr_t address);

        const device_config *m_conf;
        std::shared_ptr<gpio_resource> m_pin;

        static inline std::array<std::optional<ds18x20_addr_t>, max_num_devices> _device_addresses;
        static inline std::shared_mutex _instance_mutex;
};