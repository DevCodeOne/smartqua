#include "soft_timer_actions.h"

#include "aq_main.h"

json_action_result get_timers_action(std::optional<unsigned int> index, char *buf, size_t buf_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    if (!index.has_value()) {
        return result;
    }

    json_out answer = JSON_OUT_BUF(buf, buf_len);

    retrieve_timer_info timer_info{ .index = static_cast<size_t>(*index) };
    global_store.read_event(timer_info);

    if (timer_info.result.collection_result == timer_collection_operation::ok && timer_info.timer_info.has_value()) {
        auto info = timer_info.timer_info.value();
        result.answer_len = json_printf(&answer, "{ data : %M }",
            json_printf_single<std::decay_t<decltype(timer_info.timer_info)::value_type>>, &(info));
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result add_timer_action(std::optional<unsigned int> index, char *in_and_out_buf, size_t buf_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    json_token token{};
    json_out answer = JSON_OUT_BUF(in_and_out_buf, buf_len);

    json_scanf(in_and_out_buf, buf_len, "{ timer : %T }", &token);

    if (token.ptr == nullptr || token.len == 0) {
        return result;
    }

    add_timer to_add{ };
    to_add.index = index;
    to_add.description = std::string_view(token.ptr, token.len);

    global_store.write_event(to_add);

    if (to_add.result.collection_result == timer_collection_operation::ok && to_add.result.result_index.has_value()) {
        result.answer_len = json_printf(&answer, "{ index : %d, info : %s}", *to_add.result.result_index, "Ok added timer");
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result remove_timer_action(unsigned int index, char *buf, size_t buf_len) {
    json_out answer = JSON_OUT_BUF(buf, buf_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::failed };
    remove_timer del_timer{ .index = static_cast<size_t>(index) };
    // TODO: json_action_result
    global_store.write_event(del_timer);

    if (del_timer.result.collection_result == timer_collection_operation::ok) {
        result.answer_len = json_printf(&answer, "{ index : %d, info : %s}", del_timer.index, "Ok deleted timer");
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}

json_action_result set_timer_action(unsigned int index, char *buf, size_t buf_len) {
    json_out answer = JSON_OUT_BUF(buf, buf_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::failed };
    json_token update_description{};
    update_timer to_update{};
    to_update.index = static_cast<size_t>(index);

    json_scanf(buf, buf_len, "{ timer : %T }", &update_description);

    if (update_description.ptr == nullptr || update_description.len == 0) {
        return result;
    }

    to_update.description = std::string_view(update_description.ptr, update_description.len);
    global_store.write_event(to_update);

    if (to_update.result.op_result == single_timer_operation_result::ok && to_update.result.collection_result == timer_collection_operation::ok) {
        result.answer_len = json_printf(&answer, "{ info : %s}", "Ok wrote new values to timer");
        result.result = json_action_result_value::successfull;
    } else {
        result.answer_len = json_printf(&answer, "{ info : %s }", "An error while writing to the timer occured");
        result.result = json_action_result_value::failed;
    }

    return result;
}