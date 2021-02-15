#include "device_actions.h"

#include "frozen.h"

#include "aq_main.h"

json_action_result get_devices_action(std::optional<unsigned int> index, char *buf, size_t buf_len) {
    json_out answer = JSON_OUT_BUF(buf, buf_len);
    int answer_len = 0;

    read_from_device single_device_value{ .index = static_cast<size_t>(*index) };
    global_store.read_event(single_device_value);

    answer_len = json_printf(&answer, "data : %M",
        json_printf_single<std::decay_t<decltype(single_device_value.read_value)>>, &single_device_value.read_value);

    return { answer_len, json_action_result_value::successfull };
}

json_action_result add_device_action(std::optional<unsigned int> index, char *in_and_out_buf, size_t buf_len) {
    json_token token;
    json_token str_token;
    std::array<char, name_length> driver_name{0};

    json_scanf(in_and_out_buf, buf_len, "{ driver_type : %T, driver_param : %T }", &str_token, &token);

    if (str_token.len > name_length) {
        return { -1, json_action_result_value::failed };
    }

    std::memcpy(driver_name.data(), str_token.ptr, std::min(static_cast<int>(name_length), str_token.len));
    
    add_device to_add {};
    to_add.index = static_cast<size_t>(*index);
    to_add.driver_name = driver_name.data();
    to_add.json_input = token.ptr;
    to_add.json_len = token.len;

    global_store.write_event(to_add);

    return { 0, json_action_result_value::successfull };
}

json_action_result remove_device_action(unsigned int index, char *buf, size_t buf_len) {
    json_out answer = JSON_OUT_BUF(buf, buf_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::successfull };
    remove_single_device del_device{ .index = static_cast<size_t>(index) };
    // TODO: json_action_result
    global_store.write_event(del_device);

    result.answer_len = json_printf(&answer, "{ %Q : %Q }", "info", "OK");
    return result;
}

json_action_result set_device_action(unsigned int index, char *buf, size_t buf_len) {  
    json_token token;

    json_scanf(buf, buf_len, "{ driver_param : %T }", &token);

    write_to_device to_write {};
    to_write.index = static_cast<size_t>(index);

    // TODO: parse device_values
    global_store.write_event(to_write);

    return json_action_result{ 0, json_action_result_value::successfull };
}
