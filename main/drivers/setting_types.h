#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <charconv>

#include "utils/json_utils.h"

enum struct single_setting_result {
    ok, failure
};

struct setting_value {
    std::optional<std::array<char, 32>> string_value;
    std::optional<int32_t> int_value;
    std::optional<float> float_val;
    std::optional<bool> bool_val;
};

struct single_setting {
    std::array<char, 16> setting_name;
    setting_value value;
};

template<>
struct read_from_json<setting_value> {
    static void read(const char *str, int len, setting_value *value) {
        json_token str_token;
        json_scanf(str, len, "%T", &str_token);

        std::string_view value_as_str(str_token.ptr, str_token.len);
        int tmp_int_value;

        if (auto result = std::from_chars(value_as_str.begin(), value_as_str.end(), tmp_int_value); result.ec == std::errc()) {
            value->int_value = tmp_int_value;
            return;
        } else if (value_as_str == "true") {
            value->bool_val = true;
        } else if (value_as_str == "false") {
            value->bool_val = true;
        } else {
            if (value_as_str.length() - 1 >= std::tuple_size_v<decltype(value->string_value)::value_type>) {
                return;
            }

            value->string_value = std::array<char, 32>{};
            std::strncpy(value->string_value->begin(), value_as_str.begin(), value->string_value->size());
        }

    }
};

template<>
struct read_from_json<single_setting> {
    static void read(const char *str, int len, single_setting *value) {
        json_token name_token;
        json_scanf(str, len, "{name : %T, value : %M}", name_token, json_scanf_single<setting_value>, value->value);
    }
};