#include "device_actions.h"

#include "actions/action_types.h"
#include "drivers/device_types.h"
#include "frozen.h"

#include "smartqua_config.h"

// TODO: add the equivalent for the other actions
bool writeDeviceValue(IndexOrName index, std::string_view input, const DeviceValues &value, bool deferSaving) {
    WriteToDevice single_device_value{ .index = index, .what = input, .write_value = value };
    globalStore->writeEvent(single_device_value, deferSaving);

    return single_device_value.result.collection_result == DeviceCollectionOperation::ok
        && single_device_value.result.op_result == DeviceOperationResult::ok;
}

std::optional<DeviceValues> readDeviceValue(IndexOrName index, std::string_view input) {
    ReadFromDevice single_device_value{ .index = index, .what = input };
    globalStore->readEvent(single_device_value);

    if (single_device_value.result.collection_result != DeviceCollectionOperation::ok
        || single_device_value.result.op_result != DeviceOperationResult::ok) {
        return std::nullopt;
    }

    return std::make_optional(single_device_value.read_value);
}

[[nodiscard]] JsonActionResult retrieve_device_overview_data(const char* output_buffer, size_t output_buffer_len, json_out answer,
                                                             JsonActionResult result)
{
    auto overview_buffer = LargeBufferPoolType::get_free_buffer();
    RetrieveDeviceOverview overview{};
    overview.output_dst = overview_buffer->data();
    overview.output_len = overview_buffer->size();

    globalStore->readEvent(overview);

    if (overview.result.collection_result == DeviceCollectionOperation::failed)
    {
        return result;
    }

    if (output_buffer == nullptr || output_buffer_len == 0)
    {
        return result;
    }
    result.answer_len = json_printf(&answer, "{ data : %s }", overview_buffer->data());
    result.result = JsonActionResultStatus::success;
    return result;
}

[[nodiscard]] JsonActionResult retrieve_single_device_read(std::optional<unsigned int> index, const char* input, size_t input_len,
                                                           const char* output_buffer, size_t output_buffer_len, json_out answer,
                                                           JsonActionResult result)
{
    ReadFromDevice single_device_value{ .index = static_cast<size_t>(*index), .what = std::string_view(input, input_len), .read_value{} };
    globalStore->readEvent(single_device_value);

    if (single_device_value.result.collection_result != DeviceCollectionOperation::ok ||
        single_device_value.result.op_result != DeviceOperationResult::ok )
    {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occurred");
        }
        result.result = JsonActionResultStatus::failed;
        return result;
    }

    if (output_buffer != nullptr && output_buffer_len != 0) {
        result.answer_len = json_printf(&answer, "{ data : %M }",
                                        json_printf_single<decltype(single_device_value.read_value)>, &single_device_value.read_value);
    }
    result.result = JsonActionResultStatus::success;

    return result;
}

JsonActionResult get_devices_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };

    if (!index.has_value()) {
        return retrieve_device_overview_data(output_buffer, output_buffer_len, answer, result);
    }

    return retrieve_single_device_read(index, input, input_len, output_buffer, output_buffer_len, answer, result);
}

JsonActionResult get_device_info(IndexOrName index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };

    auto info_buffer = LargeBufferPoolType::get_free_buffer();
    RetrieveDeviceInfo info{ .index = index };
    info.output_dst = info_buffer->data();
    info.output_len = info_buffer->size();

    globalStore->readEvent(info);

    if (info.result.collection_result != DeviceCollectionOperation::ok || info.result.op_result != DeviceOperationResult::ok)
    {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occurred");
        }
        result.result = JsonActionResultStatus::failed;
        return result;
    }

    if (output_buffer != nullptr && output_buffer_len != 0) {
        result.answer_len = json_printf(&answer, "{ data : %s }", info_buffer->data());
    }
    result.result = JsonActionResultStatus::success;


    return result;
}

JsonActionResult add_device_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };
    json_token token{};
    json_token driver_type_token{};
    json_token description_token{};

    json_scanf(input, input_len, "{ description : %T, driver_type : %T, driver_param : %T }", &description_token, &driver_type_token, &token);

    if (driver_type_token.ptr == nullptr || driver_type_token.len > name_length) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "Driver name was too long or non existent");
        }
        return result;
    }

    if (description_token.ptr == nullptr || description_token.len > name_length) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "Description was too long or not existing");
        }
        return result;
    }

    IndexOrNameOrEmpty toSetOrAdd{std::monostate{}};

    if (index)
    {
        toSetOrAdd = *index;
    }

    AddDevice to_add {};
    to_add.index = toSetOrAdd;
    to_add.driver_name = tokenToStringView(driver_type_token, name_length);
    to_add.jsonSettingValue = tokenToStringView(token);
    to_add.settingName = tokenToStringView(description_token);

    globalStore->writeEvent(to_add);

    if (to_add.result.collection_result != DeviceCollectionOperation::ok || !to_add.result.index.has_value())
    {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occurred");
        }
        result.result = JsonActionResultStatus::failed;
        return result;
    }

    if (output_buffer != nullptr && output_buffer_len != 0) {
        result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", *to_add.result.index, "Ok added device");
    }
    result.result = JsonActionResultStatus::success;
    return result;
}

JsonActionResult remove_device_action(IndexOrName index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result { .answer_len = 0, .result = JsonActionResultStatus::failed };
    RemoveSingleDevice del_device{ .index = index };
    // TODO: json_action_result
    globalStore->writeEvent(del_device);

    if (del_device.result.collection_result == DeviceCollectionOperation::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            if (auto asIndex = getOpt<unsigned int>(del_device.index); asIndex)
            {
                result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", asIndex.value(), "Ok deleted device");
            } else if (auto asStringView = getOpt<std::string_view>(del_device.index); asStringView)
            {
                result.answer_len = json_printf(&answer, "{ index : %s, info : %Q}", asStringView.value().data(), "Ok deleted device");
            }
        }
        result.result = JsonActionResultStatus::success;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occurred");
        }
        result.result = JsonActionResultStatus::failed;
    }

    return result;
}

JsonActionResult set_device_action(IndexOrName index, std::string_view input, char *deviceValueInput, size_t deviceValueLen, char *output_buffer, size_t output_buffer_len) {
    DeviceValues value;

    json_scanf(deviceValueInput, deviceValueLen, "%M", json_scanf_single<decltype(value)>, &value);

    return set_device_action(index, input, value, output_buffer, output_buffer_len);
}

// TODO: rename this method
JsonActionResult set_device_action(IndexOrName index, std::string_view what, const DeviceValues &value, char *output_buffer, size_t output_buffer_len) {
    JsonActionResult result { .answer_len = 0, .result = JsonActionResultStatus::failed };
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    WriteToDevice to_write {
        .index = index,
        .what = what,
        .write_value = value
    };

    globalStore->writeEvent(to_write);

    if (to_write.result.op_result == DeviceOperationResult::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q}", "Ok wrote value to device");
        }
        result.result = JsonActionResultStatus::success;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error while writing to device occurred");
        }
        result.result = JsonActionResultStatus::failed;
    }

    return result;
}

JsonActionResult write_device_options_action(IndexOrName index, const char *action, char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    JsonActionResult result{ 0, JsonActionResultStatus::failed };

    auto info_buffer = LargeBufferPoolType::get_free_buffer();
    // TODO: check if this fixes the issue
    std::memset(info_buffer->data(), 0, info_buffer->size());

    WriteDeviceOptions info{
        .index = index,
        .action = action,
        .input = std::string_view{input, input_len},
        .output_dst = info_buffer->data(),
        .output_len = info_buffer->size()};

    globalStore->writeEvent(info);

    if (info.result.collection_result != DeviceCollectionOperation::ok || info.result.op_result != DeviceOperationResult::ok)
    {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occurred");
        }
        result.result = JsonActionResultStatus::failed;
    }

    if (output_buffer != nullptr && output_buffer[0] != '\0' && output_buffer_len != 0) {
        result.answer_len = json_printf(&answer, "{ data : %s }", info_buffer->data());
    }
    result.result = JsonActionResultStatus::success;

    return result;
}