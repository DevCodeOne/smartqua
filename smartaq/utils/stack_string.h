#pragma once

#include <cstddef>
#include <array>
#include <cstring>
#include <string_view>
#include <type_traits>

// TODO: maybe put this function in an own file
template<typename CharT>
size_t safe_strlen(const CharT *data, size_t maxLength) {
    for (int i = 0; i < maxLength; ++i) {
        if (data[i] == '\0') {
            return i;
        }
    }

    return maxLength;
}

namespace String {
    // TODO: rename len to length
    template<typename CharT, size_t Size>
    struct BasicStackString : public std::array<CharT, Size> {

        static constexpr size_t ArrayCapacity = Size;

        BasicStackString() = default;
        BasicStackString(const std::string_view &other);
        BasicStackString(const char *other);

        template<size_t OtherSize, typename = std::enable_if_t<OtherSize <= Size>>
        BasicStackString &operator=(const BasicStackString<CharT, OtherSize> &other);
        BasicStackString &operator=(const std::string_view &other);
        BasicStackString &operator=(const char *other);
        template<size_t OtherSize, typename = std::enable_if_t<OtherSize <= Size>>
        BasicStackString &operator+=(const BasicStackString<CharT, OtherSize> &other);
        BasicStackString &operator+=(const std::string_view &other);
        BasicStackString &operator+=(const char *other);


        size_t capacity() const;
        size_t len() const;
        size_t length() const;

        std::string_view getStringView() const;
        operator std::string_view() const;

    };

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size>::BasicStackString(const std::string_view &other) {
        operator=(other);
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size>::BasicStackString(const char *other) {
        operator=(other);
    }

    // TODO: think about this, off by one error possibly, also probably use the other size ?
    template<typename CharT, size_t Size>
    template<size_t OtherSize, typename>
    BasicStackString<CharT, Size> &BasicStackString<CharT, Size>::operator=(const BasicStackString<CharT, OtherSize> &other) {
        std::memcpy(this->data(), other.data(), Size);
        this->data()[Size - 1] = '\0';

        return *this;
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size> &BasicStackString<CharT, Size>::operator=(const std::string_view &other) {
        const auto toCopy = std::min(Size - 1, other.size());
        std::memcpy(this->data(), other.data(), toCopy);
        this->data()[toCopy] = '\0';

        return *this;
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size> &BasicStackString<CharT, Size>::operator=(const char *other) {
        const auto toCopy = std::min(Size - 1, safe_strlen(other, Size));
        std::memcpy(this->data(), other, toCopy);
        this->data()[toCopy] = '\0';

        return *this;
    }


    template<typename CharT, size_t Size>
    size_t BasicStackString<CharT, Size>::capacity() const {
        return Size;
    }

    template<typename CharT, size_t Size>
    size_t BasicStackString<CharT, Size>::len() const {
        return safe_strlen(this->data(), Size);
    }


    template<typename CharT, size_t Size>
    size_t BasicStackString<CharT, Size>::length() const {
        return safe_strlen(this->data(), Size);
    }

    template<typename CharT, size_t Size>
    std::string_view BasicStackString<CharT, Size>::getStringView() const {
        return std::string_view(this->data(), this->len());
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size>::operator std::string_view() const {
        return getStringView();
    }
}

template<size_t Size>
using BasicStackString = String::BasicStackString<char, Size>;