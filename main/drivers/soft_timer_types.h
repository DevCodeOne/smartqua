#pragma once

#include <cstdint>
#include <array>
#include <chrono>

#include "frozen.h"

#include "smartqua_config.h"
#include "utils/json_utils.h"

enum struct single_timer_operation_result {
    ok, failure
};

struct single_timer_settings {
    uint32_t time_of_day;
    uint32_t device_index;
    bool enabled;
    int8_t weekday_mask;
    std::array<char, max_action_payload_length> payload;
};

template<>
struct print_to_json<single_timer_settings> {
    static int print(json_out *out, const single_timer_settings &settings) {
        return json_printf(out, "{ time_of_day : %u, enabled : %B weekday_mask : %u, device_index : %u, payload : %s }",
            settings.time_of_day, settings.enabled, static_cast<uint32_t>(settings.weekday_mask), settings.device_index, settings.payload.data());
    }
};