#pragma once

#include <cstddef>
#include <array>
#include <cstring>
#include <string_view>
#include <type_traits>

// TODO: maybe put this function in an own file
inline size_t safe_strlen(const char *data, size_t maxLength) {
    for (int i = 0; i < maxLength; ++i) {
        if (data[i] == '\0') {
            return i;
        }
    }

    return maxLength;
}

template<typename CharT, size_t Size>
struct basic_stack_string : public std::array<CharT, Size> {

    static inline constexpr size_t ArrayCapacity = Size;

    basic_stack_string() = default;
    basic_stack_string(const std::string_view &other);
    basic_stack_string(const char *other);

    template<size_t OtherSize, typename = std::enable_if_t<OtherSize <= Size>>
    basic_stack_string &operator=(const basic_stack_string<CharT, OtherSize> &other);
    basic_stack_string &operator=(const std::string_view &other);
    basic_stack_string &operator=(const char *other);

    size_t capacity() const;
    size_t len() const;

    std::string_view getStringView() const;

};

template<typename CharT, size_t Size>
basic_stack_string<CharT, Size>::basic_stack_string(const std::string_view &other) {
    operator=(other);
}

template<typename CharT, size_t Size>
basic_stack_string<CharT, Size>::basic_stack_string(const char *other) {
    operator=(other);
}

// TODO: think about this, off by one error possibly, also probably use the other size ?
template<typename CharT, size_t Size>
template<size_t OtherSize, typename>
basic_stack_string<CharT, Size> &basic_stack_string<CharT, Size>::operator=(const basic_stack_string<CharT, OtherSize> &other) {
    std::memcpy(this->data(), other.data(), Size);
    this->data()[Size - 1] = '\0';

    return *this;
}

template<typename CharT, size_t Size>
basic_stack_string<CharT, Size> &basic_stack_string<CharT, Size>::operator=(const std::string_view &other) {
    const auto toCopy = std::min(Size - 1, other.size());
    std::memcpy(this->data(), other.data(), toCopy);
    this->data()[toCopy] = '\0';

    return *this;
}

template<typename CharT, size_t Size>
basic_stack_string<CharT, Size> &basic_stack_string<CharT, Size>::operator=(const char *other) {
    const auto toCopy = std::min(Size - 1, safe_strlen(other, Size));
    std::memcpy(this->data(), other, toCopy);
    this->data()[toCopy] = '\0';

    return *this;
}


template<typename CharT, size_t Size>
size_t basic_stack_string<CharT, Size>::capacity() const {
    return Size;
}

template<typename CharT, size_t Size>
size_t basic_stack_string<CharT, Size>::len() const {
    return safe_strlen(this->data(), Size);
}

template<typename CharT, size_t Size>
std::string_view basic_stack_string<CharT, Size>::getStringView() const {
    return std::string_view(this->data(), this->len());
}

template<size_t Size>
using stack_string = basic_stack_string<char, Size>;