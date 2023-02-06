#pragma once

#include <cstdarg>
#include <array>
#include <utility>
#include <charconv>
#include <cstring>
#include <optional>
#include <string_view>

#include "frozen.h"

#include "utils/stack_string.h"

#include <esp_log.h>

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
void json_scanf_single(const char *str, int len, void *user_data);

template<typename T>
int json_printf_single(json_out *out, va_list *ap);

// TODO: add parse result
template<typename T>
struct read_from_json;

template<>
struct read_from_json<int> { 
    static void read(const char *str, int len, int &user_data) {
        std::from_chars(str, str + len, user_data);
    }
};

template<size_t Size>
struct read_from_json<stack_string<Size>> { 
    static void read(const char *str, int len, stack_string<Size> &user_data) {
        user_data = std::string_view(str, len);
    }
};

// This will only be valid, as long as the buffer isn't changed, still usefull
template<>
struct read_from_json<std::string_view> { 
    static void read(const char *str, int len, std::string_view &user_data) {
        user_data = std::string_view(str, len);
    }
};

template<typename T>
struct read_from_json<std::optional<T>> {
    static void read(const char *str, int len, std::optional<T> &user_data) {
        T to_store{};
        read_from_json<T>::read(str, len, to_store);
        user_data = to_store;
    }
};

template<typename T>
struct print_to_json;

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

template<size_t Size>
struct print_to_json<stack_string<Size>> {
    static int print(json_out *out, stack_string<Size> &str) {
        return json_printf(out, "%.*Q", str.len(), str.data());
    }
};

template<>
struct print_to_json<std::string_view> {
    static int print(json_out *out, std::string_view &str) {
        return json_printf(out, "%.*Q", str.length(), str.data());
    }
};

template<typename T>
void json_scanf_single(const char *str, int len, void *user_data) {
    using decayed_type = std::decay_t<T>;
    read_from_json<decayed_type>::read(str, len, 
        *reinterpret_cast<std::add_pointer_t<decayed_type>>(user_data));
}

template<typename ArrayType, typename ValueType = typename ArrayType::value_type, auto Size = std::tuple_size<ArrayType>::value>
void json_scanf_array(const char *str, int len, void *user_data) {
    int i;
    json_token currentElement{};
    ArrayType *const value = reinterpret_cast<ArrayType *>(user_data);

    for (i = 0; json_scanf_array_elem(str, len, "", i, &currentElement) > 0 && i < Size; ++i) {
        json_scanf_single<ValueType>(currentElement.ptr, currentElement.len, reinterpret_cast<void *>(&value->data()[i]));
    }
}


template<typename T>
int json_printf_single(json_out *out, va_list *ap) {
    T *const value = va_arg(*ap, T*);
    return print_to_json<std::remove_pointer_t<T>>::print(out, *value);
}

template<typename ArrayType, typename ValueType = typename ArrayType::value_type, auto Size = std::tuple_size<ArrayType>::value>
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