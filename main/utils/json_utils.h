#pragma once

#include <cstdarg>
#include <array>
#include <utility>
#include <charconv>
#include <cstring>

#include "frozen.h"
#include "esp_log.h"

template<typename T1, typename T2>
constexpr auto constexpr_pow(T1 n, T2 e) {
    static_assert(std::is_integral_v<T1> && std::is_integral_v<T2>);
    if (e == 0) {
        return 1;
    }

    if (e == 1) {
        return n;
    }

    return n * constexpr_pow(n, e - 1);
}

template<typename FloatType, typename IntTypes = int, size_t AfterPoint = 2>
std::pair<IntTypes, IntTypes> fixed_decimal_rep(FloatType value) {
    const auto integer_value = static_cast<IntTypes>(value);
    return std::make_pair(integer_value, (value - integer_value) * constexpr_pow(10, AfterPoint));
}

template<typename T>
struct read_from_json { };

template<typename T>
struct print_to_json { };

template<typename T>
struct print_to_json<std::optional<T>> {
    static int print(json_out *out, std::optional<T> &opt) {
        if (!opt.has_value()) {
            return 0;
        }

        return print_to_json<T>::print(out, *opt);
    }
};

template<size_t Size>
struct print_to_json<std::array<char, Size>> {
    static int print(json_out *out, std::array<char, Size> &str) {
        size_t len = 0;
        if (str[len - 1] != '\0') {
            len = str.size() - 1;
        } else {
            len = strlen(str.data());
        }

        return json_printf(out, "%.*Q", len, str.data());
    }
};

template<typename T>
void json_scanf_single(const char *str, int len, void *user_data) {
    using decayed_type = std::decay_t<T>;
    read_from_json<decayed_type>::read(str, len, 
        reinterpret_cast<std::add_pointer_t<decayed_type>>(user_data));
}


template<typename T>
int json_printf_single(json_out *out, va_list *ap) {
    T *value = va_arg(*ap, T*);
    return print_to_json<std::remove_pointer_t<T>>::print(out, *value);
}

template<typename ArrayType>
int json_printf_array(json_out *out, va_list *ap) {
    static std::array<const char *, 2> formats{" %M", ", %M "};
    const auto arr = reinterpret_cast<ArrayType *>(va_arg(*ap, void *));

    int printed_bytes = 0;

    printed_bytes += json_printf(out, "[");
    int printed_array = 0;
    for (size_t i = 0; i < arr->size(); ++i) {
        printed_array += json_printf(out, formats[printed_array > 0], json_printf_single<typename ArrayType::value_type>, &arr->data()[i]);
    }
    printed_bytes += json_printf(out, "]");
    printed_bytes += printed_array;

    return printed_bytes;
}