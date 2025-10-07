#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <cmath>
#include <compare>
#include <type_traits>
#include <variant>

#include "build_config.h"

enum struct DeviceOperationResult {
    ok, not_supported, failure
};

struct DeviceConfig { 
    static constexpr char StorageName[] = "DeviceConfig";

    BasicStackString<name_length> device_driver_name;
    // TODO: add method to write driver conf
    mutable std::array<char, device_config_size> device_config; // Binary data

    template<typename TargetType>
    requires (sizeof(TargetType) < device_config_size)
    TargetType *accessConfig() const requires (!std::is_pointer_v<TargetType>) {
        return reinterpret_cast<TargetType *>(device_config.data());
    }

    template<typename TargetType>
    requires (sizeof(device_config_size) <= sizeof(TargetType))
    void insertConfig(const TargetType *type) {
        std::memcpy(device_config.data(), type, sizeof(TargetType));
    }

};

#include "utils/device_values.h"
#include "utils/serialization/device_values_serialization.h"
