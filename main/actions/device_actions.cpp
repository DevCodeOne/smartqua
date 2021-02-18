#include "device_actions.h"

#include "frozen.h"

#include "aq_main.h"

json_action_result get_devices_action(std::optional<unsigned int> index, char *buf, size_t buf_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    if (!index.has_value()) {
        return result;
    }

    json_out answer = JSON_OUT_BUF(buf, buf_len);

    read_from_device single_device_value{ .index = static_cast<size_t>(*index) };
    global_store.read_event(single_device_value);

    if (single_device_value.result.collection_result == device_collection_operation::ok &&
            single_device_value.result.op_result == device_operation_result::ok ) {
        result.answer_len = json_printf(&answer, "{ data : %M }",
            json_printf_single<std::decay_t<decltype(single_device_value.read_value)>>, &single_device_value.read_value);
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result add_device_action(std::optional<unsigned int> index, char *in_and_out_buf, size_t buf_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    json_token token;
    json_token str_token;
    std::array<char, name_length> driver_name{0};
    json_out answer = JSON_OUT_BUF(in_and_out_buf, buf_len);

    json_scanf(in_and_out_buf, buf_len, "{ driver_type : %T, driver_param : %T }", &str_token, &token);

    if (str_token.ptr == nullptr || str_token.len > name_length) {
        result.answer_len = json_printf(&answer, "{ info : %s}", "Drivername was too long");
        return result;
    }

    std::memcpy(driver_name.data(), str_token.ptr, std::min(static_cast<int>(name_length), str_token.len));
    
    add_device to_add {};
    to_add.index = index;
    to_add.driver_name = driver_name.data();
    to_add.json_input = token.ptr;
    to_add.json_len = token.len;

    global_store.write_event(to_add);

    if (to_add.result.collection_result == device_collection_operation::ok && to_add.result.result_index.has_value()) {
        result.answer_len = json_printf(&answer, "{ index : %d, info : %s}", *to_add.result.result_index, "Ok added device");
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result remove_device_action(unsigned int index, char *buf, size_t buf_len) {
    json_out answer = JSON_OUT_BUF(buf, buf_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::failed };
    remove_single_device del_device{ .index = static_cast<size_t>(index) };
    // TODO: json_action_result
    global_store.write_event(del_device);

    if (del_device.result.collection_result == device_collection_operation::ok) {
        result.answer_len = json_printf(&answer, "{ index : %d, info : %s}", del_device.index, "Ok deleted device");
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result set_device_action(unsigned int index, char *buf, size_t buf_len) {  
    json_out answer = JSON_OUT_BUF(buf, buf_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::successfull };
    write_to_device to_write {};
    to_write.index = static_cast<size_t>(index);

    json_scanf(buf, buf_len, "%M", json_scanf_single<decltype(to_write.write_value)>, &to_write.write_value);

    // TODO: parse device_values
    global_store.write_event(to_write);

    if (to_write.result.op_result == device_operation_result::ok) {
        result.answer_len = json_printf(&answer, "{ info : %s}", "Ok wrote value to device");
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error while writing to device occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}
