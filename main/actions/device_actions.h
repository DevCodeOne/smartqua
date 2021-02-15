#pragma once

#include <string_view>
#include <optional>

#include "drivers/device_types.h"

enum struct json_action_result_value {
    successfull, not_found, failed
};

struct json_action_result  {
    int answer_len = -1;
    json_action_result_value result;
};

json_action_result get_devices_action(std::optional<unsigned int> index, char *buf, size_t buf_len);
json_action_result add_device_action(std::optional<unsigned int> index, char *buf, size_t buf_len);
json_action_result remove_device_action(unsigned int index, char *buf, size_t buf_len);
json_action_result set_device_action(unsigned int index, char *buf, size_t buf_len);
