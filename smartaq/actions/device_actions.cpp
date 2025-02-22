#include "device_actions.h"

#include "actions/action_types.h"
#include "drivers/device_types.h"
#include "frozen.h"

#include "smartqua_config.h"

// TODO: add the equivalent for the other actions
bool writeDeviceValue(unsigned int index, std::string_view input, const DeviceValues &value, bool deferSaving) {
    WriteToDevice single_device_value{ .index = index, .what = input, .write_value = value };
    global_store->writeEvent(single_device_value, deferSaving);

    if (single_device_value.result.collection_result != DeviceCollectionOperation::ok
        || single_device_value.result.op_result != DeviceOperationResult::ok) {
        return false;
    }

    return true;
}

std::optional<DeviceValues> readDeviceValue(unsigned int index, std::string_view input) {
    ReadFromDevice single_device_value{ .index = index, .what = input };
    global_store->readEvent(single_device_value);

    if (single_device_value.result.collection_result != DeviceCollectionOperation::ok
        || single_device_value.result.op_result != DeviceOperationResult::ok) {
        return std::nullopt;
    }

    return std::make_optional(single_device_value.read_value);
}

JsonActionResult get_devices_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };

    if (!index.has_value()) {
        auto overview_buffer = LargeBufferPoolType::get_free_buffer();
        RetrieveDeviceOverview overview{};
        overview.output_dst = overview_buffer->data();
        overview.output_len = overview_buffer->size();

        global_store->readEvent(overview);

        if (overview.result.collection_result != DeviceCollectionOperation::failed) {
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ data : %s }", overview_buffer->data());
            }
            result.result = JsonActionResultStatus::success;
        }
    } else {
        ReadFromDevice single_device_value{ .index = static_cast<size_t>(*index), .what = std::string_view(input, input_len), .read_value{} };
        global_store->readEvent(single_device_value);

        if (single_device_value.result.collection_result == DeviceCollectionOperation::ok &&
                single_device_value.result.op_result == DeviceOperationResult::ok ) {
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ data : %M }",
                    json_printf_single<decltype(single_device_value.read_value)>, &single_device_value.read_value);
            }
            result.result = JsonActionResultStatus::success;
        } else {
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
            }
            result.result = JsonActionResultStatus::failed;
        }
    }

    return result;
}

JsonActionResult get_device_info(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };

    auto info_buffer = LargeBufferPoolType::get_free_buffer();
    RetrieveDeviceInfo info{ .index = static_cast<unsigned int>(index) };
    info.output_dst = info_buffer->data();
    info.output_len = info_buffer->size();

    global_store->readEvent(info);

    if (info.result.collection_result == DeviceCollectionOperation::ok && info.result.op_result == DeviceOperationResult::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ data : %s }", info_buffer->data());
        }
        result.result = JsonActionResultStatus::success;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
        }
        result.result = JsonActionResultStatus::failed;
    }

    return result;
}

JsonActionResult add_device_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };
    json_token token;
    json_token driver_type_token;
    json_token description_token;

    json_scanf(input, input_len, "{ description : %T, driver_type : %T, driver_param : %T }", &description_token, &driver_type_token, &token);

    if (driver_type_token.ptr == nullptr || driver_type_token.len > name_length) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "Drivername was too long or non existent");
        }
        return result;
    }

    if (description_token.ptr == nullptr || description_token.len > name_length) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "Description was too long or not existing");
        }
        return result;
    }

    add_device to_add { };
    to_add.index = index;
    to_add.driver_name = std::string_view(driver_type_token.ptr, std::min(static_cast<int>(name_length), driver_type_token.len));
    to_add.jsonSettingValue = std::string_view(token.ptr, token.len);
    to_add.settingName = std::string_view(description_token.ptr, std::min(static_cast<int>(name_length), description_token.len));

    global_store->writeEvent(to_add);

    if (to_add.result.collection_result == DeviceCollectionOperation::ok && to_add.result.index.has_value()) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", *to_add.result.index, "Ok added device");
        }
        result.result = JsonActionResultStatus::success;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
        }
        result.result = JsonActionResultStatus::failed;
    }

    return result;
}

JsonActionResult remove_device_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result { .answer_len = 0, .result = JsonActionResultStatus::failed };
    RemoveSingleDevice del_device{ .index = static_cast<size_t>(index) };
    // TODO: json_action_result
    global_store->writeEvent(del_device);

    if (del_device.result.collection_result == DeviceCollectionOperation::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", *del_device.index, "Ok deleted device");
        }
        result.result = JsonActionResultStatus::success;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
        }
        result.result = JsonActionResultStatus::failed;
    }

    return result;
}

JsonActionResult set_device_action(unsigned int index, std::string_view input, char *deviceValueInput, size_t deviceValueLen, char *output_buffer, size_t output_buffer_len) {  
    DeviceValues value;

    json_scanf(deviceValueInput, deviceValueLen, "%M", json_scanf_single<decltype(value)>, &value);

    return set_device_action(index, input, value, output_buffer, output_buffer_len);
}

// TODO: rename this method
JsonActionResult set_device_action(unsigned int index, std::string_view what, const DeviceValues &value, char *output_buffer, size_t output_buffer_len) {
    JsonActionResult result { .answer_len = 0, .result = JsonActionResultStatus::failed };
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    WriteToDevice to_write {
        .index = static_cast<size_t>(index),
        .what = what,
        .write_value = value
    };

    global_store->writeEvent(to_write);

    if (to_write.result.op_result == DeviceOperationResult::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q}", "Ok wrote value to device");
        }
        result.result = JsonActionResultStatus::success;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error while writing to device occured");
        }
        result.result = JsonActionResultStatus::failed;
    }

    return result;
}

JsonActionResult write_device_options_action(unsigned int index, const char *action, char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };

    auto info_buffer = LargeBufferPoolType::get_free_buffer();
    // TODO: check if this fixes the issue
    std::memset(info_buffer->data(), 0, info_buffer->size());

    WriteDeviceOptions info{
        .index = static_cast<unsigned int>(index), 
        .action = action,
        .input = std::string_view{input, input_len},
        .output_dst = info_buffer->data(),
        .output_len = info_buffer->size()};

    global_store->writeEvent(info);

    if (info.result.collection_result == DeviceCollectionOperation::ok && info.result.op_result == DeviceOperationResult::ok) {
        if (output_buffer != nullptr && output_buffer[0] != '\0' && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ data : %s }", info_buffer->data());
        }
        result.result = JsonActionResultStatus::success;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
        }
        result.result = JsonActionResultStatus::failed;
    }

    return result;
}