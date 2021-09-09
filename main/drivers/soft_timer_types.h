#pragma once

#include <cstdint>
#include <array>
#include <chrono>

#include "frozen.h"

#include "smartqua_config.h"
#include "utils/json_utils.h"
#include "utils/stack_string.h"

enum struct single_timer_operation_result {
    ok, failure
};

struct single_timer_settings {
    static inline constexpr char StorageName[] = "timer_settings";

    uint32_t time_of_day;
    uint32_t device_index;
    bool enabled;
    int8_t weekday_mask;
    // TODO: won't work is too long write to sd card and put path here
    bool largePayload;
    stack_string<max_action_payload_length> payload;
};

template<>
struct print_to_json<single_timer_settings> {
    static int print(json_out *out, const single_timer_settings &settings) {
        return json_printf(out, "{ time_of_day : %u, enabled : %B, weekday_mask : %u, device_index : %u, large_payload : %B, payload : %s }",
            settings.time_of_day, 
            settings.enabled,
            static_cast<uint32_t>(settings.weekday_mask),
            settings.device_index,
            settings.largePayload,
            settings.payload.data());
    }
};