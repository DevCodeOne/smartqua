#pragma once

#include <string_view>
#include <optional>

#include "device_types.h"

class ds18x20_driver {
    static inline constexpr char name[] = "ds18x20_driver";

    public:
        static std::optional<ds18x20_driver> create_driver(const device_config *conf);

        device_write_result write_value(const device_values &value);
        device_read_result read_value(device_values &value);
    private:
        ds18x20_driver(const device_config *conf);

        const device_config *const m_conf;
};