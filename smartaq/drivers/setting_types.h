#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <charconv>

#include "utils/stack_string.h"
#include "utils/trivial_variant.h"
#include "utils/serialization/json_utils.h"

enum struct single_setting_result {
    Ok, Failure
};

enum class SettingNames {
    Hostname
};

struct SingleSetting {
    static constexpr char StorageName[] = "Setting";

    using StringValueType = BasicStackString<16>;

    StringValueType name;
    TrivialVariant<StringValueType, int32_t, bool> value;
};

static void readSettingValue(const char *str, int len, SingleSetting &value) {
    json_token str_token;
    json_scanf(str, len, "%T", &str_token);

    std::string_view value_as_str(str_token.ptr, str_token.len);
    int32_t tmp_int_value;

    if (auto result = std::from_chars(value_as_str.begin(), value_as_str.end(), tmp_int_value);
        result.ec == std::errc()) {
        value.value.setValue(tmp_int_value);
        return;
    }
    if (value_as_str == "true") {
        value.value.setValue(true);
        return;
    }
    if (value_as_str == "false") {
        value.value.setValue(false);
        return;
    }

    // TODO: check if stack_string fails, when assigning a string, which is too long
    value.value.setValue(SingleSetting::StringValueType(value_as_str));
}

template<>
struct read_from_json<SingleSetting> {
    static void read(const char *str, int len, SingleSetting *value) {
        json_token name_token;
        json_token value_token;
        json_scanf(str, len, "{name : %T, value : %T}", &name_token, &value_token);

        value->name = std::string_view(name_token.ptr, name_token.len);
        readSettingValue(value_token.ptr, value_token.len, *value);
    }
};