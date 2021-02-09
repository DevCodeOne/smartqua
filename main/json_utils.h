#pragma once

#include <cstdarg>
#include <array>

#include "frozen.h"

template<typename T>
struct print_to_json { };

template<size_t Size>
struct print_to_json<std::array<char, Size>> {
    static int print(json_out *out, std::array<char, Size> &str) {
        return json_printf(out, "%.*Q", str, str.size());
    }
};

template<typename T>
int json_printf_single(json_out *out, va_list *ap) {
    T *value = va_arg(*ap, T*);
    return print_to_json<std::remove_pointer_t<T>>::print(out, *value);
}

template<typename ArrayType>
int json_printf_array(json_out *out, va_list *ap) {
    static std::array<const char *, 2> formats{"%M, ", "%M"};
    const auto arr = reinterpret_cast<ArrayType *>(va_arg(*ap, void *));

    int printed_bytes = 0;

    printed_bytes += json_printf(out, "[");
    for (size_t i = 0; i < arr->size(); ++i) {
        printed_bytes += json_printf(out, formats[i == arr->size() - 1], json_printf_single<typename ArrayType::value_type>, &arr->data()[i]);
    }
    printed_bytes += json_printf(out, "]");

    return printed_bytes;
}