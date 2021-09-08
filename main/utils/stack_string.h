#pragma once

#include <cstddef>
#include <array>
#include <cstring>
#include <string_view>
#include <type_traits>

template<typename CharT, size_t Size>
struct basic_stack_string : public std::array<CharT, Size> {

    template<size_t OtherSize, typename = std::enable_if_t<OtherSize <= Size>>
    basic_stack_string &operator=(const basic_stack_string<CharT, OtherSize> &other);
    basic_stack_string &operator=(const std::string_view &other);

    size_t capacity() const;
    size_t len() const;

};

template<typename CharT, size_t Size>
template<size_t OtherSize, typename>
basic_stack_string<CharT, Size> &basic_stack_string<CharT, Size>::operator=(const basic_stack_string<CharT, OtherSize> &other) {
    std::memcpy(this->data(), other.data(), Size);
    this->data()[Size - 1] = '\0';

    return *this;
}

template<typename CharT, size_t Size>
basic_stack_string<CharT, Size> &basic_stack_string<CharT, Size>::operator=(const std::string_view &other) {
    std::memcpy(this->data(), other.data(), std::min<int>(Size, other.size()));
    if (other.size() < Size) {
        this->data()[other.size()] = '\0';
    }
    this->data()[Size - 1] = '\0';

    return *this;
}


template<typename CharT, size_t Size>
size_t basic_stack_string<CharT, Size>::capacity() const {
    return Size;
}

template<typename CharT, size_t Size>
size_t basic_stack_string<CharT, Size>::len() const {
    for (int i = 0; i < Size; ++i) {
        if (this->data()[i] == '\0') {
            return i;
        }
    }

    return Size;
}

template<size_t Size>
using stack_string = basic_stack_string<char, Size>;