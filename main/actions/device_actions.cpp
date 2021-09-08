#include "device_actions.h"

#include "frozen.h"

#include "aq_main.h"

json_action_result get_devices_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);

    if (!index.has_value()) {
        std::array<char, 1024> overview_buffer;
        retrieve_device_overview overview{};
        overview.output_dst = overview_buffer.data();
        overview.output_len = overview_buffer.size();

        global_store.read_event(overview);

        if (overview.result.collection_result != device_collection_operation::failed) {
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ data : %s }", overview_buffer.data());
            }
            result.result = json_action_result_value::successfull;
        }
    } else {
        read_from_device single_device_value{ .index = static_cast<size_t>(*index) };
        global_store.read_event(single_device_value);

        if (single_device_value.result.collection_result == device_collection_operation::ok &&
                single_device_value.result.op_result == device_operation_result::ok ) {
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ data : %M }",
                    json_printf_single<std::decay_t<decltype(single_device_value.read_value)>>, &single_device_value.read_value);
            }
            result.result = json_action_result_value::successfull;
        } else {
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
            }
            result.result = json_action_result_value::failed;
        }
    }

    return result;
}

json_action_result get_device_info(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);

    std::array<char, 1024> info_buffer;
    retrieve_device_info info{ .index = static_cast<unsigned int>(index) };
    info.output_dst = info_buffer.data();
    info.output_len = info_buffer.size();

    global_store.read_event(info);

    if (info.result.collection_result == device_collection_operation::ok && info.result.op_result == device_operation_result::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ data : %s }", info_buffer.data());
        }
        result.result = json_action_result_value::successfull;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
        }
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result add_device_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    json_token token;
    json_token driver_type_token;
    json_token description_token;
    std::array<char, name_length> driver_name{0};
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);

    json_scanf(input, input_len, "{ description : %T, driver_type : %T, driver_param : %T }", &description_token, &driver_type_token, &token);

    if (driver_type_token.ptr == nullptr || driver_type_token.len > name_length) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "Drivername was too long or non existent");
        }
        return result;
    }

    if (description_token.ptr == nullptr || description_token.len > name_length || description_token.len <= 2) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "Description was too long or not existing");
        }
        return result;
    }

    std::memcpy(driver_name.data(), driver_type_token.ptr, std::min(static_cast<int>(name_length), driver_type_token.len));
    
    add_device to_add { };
    to_add.index = index;
    to_add.driver_name = driver_name.data();
    to_add.jsonSettingValue = std::string_view(token.ptr, token.len);
    to_add.settingName = std::string_view(description_token.ptr, std::min(static_cast<int>(name_length), description_token.len));

    global_store.write_event(to_add);

    if (to_add.result.collection_result == device_collection_operation::ok && to_add.result.index.has_value()) {
        result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", *to_add.result.index, "Ok added device");
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result remove_device_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::failed };
    remove_single_device del_device{ .index = static_cast<size_t>(index) };
    // TODO: json_action_result
    global_store.write_event(del_device);

    if (del_device.result.collection_result == device_collection_operation::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", *del_device.index, "Ok deleted device");
        }
        result.result = json_action_result_value::successfull;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error occured");
        }
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result set_device_action(unsigned int index, char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {  
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::failed };
    write_to_device to_write {};
    to_write.index = static_cast<size_t>(index);

    json_scanf(input, input_len, "%M", json_scanf_single<decltype(to_write.write_value)>, &to_write.write_value);

    global_store.write_event(to_write);

    if (to_write.result.op_result == device_operation_result::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q}", "Ok wrote value to device");
        }
        result.result = json_action_result_value::successfull;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error while writing to device occured");
        }
        result.result = json_action_result_value::failed;
    }

    return result;
}
