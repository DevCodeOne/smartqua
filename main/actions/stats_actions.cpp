#include "stats_actions.h"

#include "aq_main.h"

json_action_result get_stats_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    if (!index.has_value()) {
        std::array<char, 1024> overview_buffer;
        retrieve_stat_overview overview{};
        overview.outputDst = overview_buffer.data();
        overview.outputLen = overview_buffer.size();

        global_store.read_event(overview);

        if (overview.Result.collection_result != stat_collection_operation::failed) {
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ data : %s }", overview_buffer.data());
            }
            result.result = json_action_result_value::successfull;
        }
    } else {
        retrieve_stat_info stat_info{ .index = static_cast<size_t>(*index) };
        global_store.read_event(stat_info);

        if (stat_info.result.collection_result == stat_collection_operation::ok && stat_info.stat_info.has_value()) {
            auto info = stat_info.stat_info.value();
            if (output_buffer != nullptr && output_buffer_len != 0) {
                result.answer_len = json_printf(&answer, "{ data : %M }",
                    json_printf_single<std::decay_t<decltype(stat_info.stat_info)::value_type>>, &(info));
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

json_action_result add_stat_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_action_result result{ 0, json_action_result_value::failed };
    json_token token{};
    json_token name_token{};
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);

    json_scanf(input, input_len, "{ description : %T, stat : %T }", &name_token, &token);

    if (token.ptr == nullptr || token.len == 0) {
        return result;
    }

    if (name_token.ptr == nullptr || name_token.len > name_length) {
        return result;
    }

    set_stat to_add{ };
    to_add.index = index;
    to_add.jsonSettingValue = std::string_view(token.ptr, token.len);
    to_add.settingName = std::string_view(name_token.ptr, std::min(static_cast<int>(name_length), name_token.len));

    global_store.write_event(to_add);

    if (to_add.result.collection_result == stat_collection_operation::ok && to_add.result.index.has_value()) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", *to_add.result.index, "Ok added stat");
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

json_action_result remove_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::failed };
    remove_stat del_stat{ .index = static_cast<size_t>(index) };

    global_store.write_event(del_stat);

    if (del_stat.result.collection_result == stat_collection_operation::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ index : %d, info : %Q}", del_stat.index, "Ok deleted stat");
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

json_action_result set_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len) {
    json_out answer = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_action_result result { .answer_len = 0, .result = json_action_result_value::failed };
    json_token update_description{};
    set_stat to_update{};
    to_update.index = static_cast<size_t>(index);

    json_scanf(input, input_len, "{ stat : %T }", &update_description);

    if (update_description.ptr == nullptr || update_description.len == 0) {
        return result;
    }

    to_update.jsonSettingValue = std::string_view(update_description.ptr, update_description.len);
    global_store.write_event(to_update);

    if (to_update.result.collection_result == stat_collection_operation::ok) {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q}", "Ok wrote new values to stat");
        }
        result.result = json_action_result_value::successfull;
    } else {
        if (output_buffer != nullptr && output_buffer_len != 0) {
            result.answer_len = json_printf(&answer, "{ info : %Q }", "An error while writing to the stat occured");
        }
        result.result = json_action_result_value::failed;
    }

    return result;
}