#pragma once

#include <cstddef>
#include <array>
#include <cstring>
#include <string_view>
#include <type_traits>

// TODO: maybe put this function in an own file
template<typename CharT>
size_t safe_strlen(const CharT *data, size_t maxLength) {
    if (data == nullptr) {
        return 0;
    }

    for (int i = 0; i < maxLength; ++i) {
        if (data[i] == '\0') {
            return i;
        }
    }

    return maxLength;
}

namespace String {
    template<typename CharT, size_t Size>
    struct BasicStackString : std::array<CharT, Size> {

        static constexpr size_t ArrayCapacity = Size;

        BasicStackString() = default;
        explicit BasicStackString(const std::string_view &other);
        explicit BasicStackString(const char *other);

        static bool canHold(size_t length);
        static bool canHold(const char *str);
        static bool canHold(const std::string_view &view);

        template<size_t OtherSize, typename = std::enable_if_t<OtherSize <= Size>>
        requires (OtherSize <= Size)
        BasicStackString &operator=(const BasicStackString<CharT, OtherSize> &other);
        BasicStackString &operator=(const std::string_view &other);
        BasicStackString &operator=(const char *other);
        template<size_t OtherSize>
        requires (OtherSize <= Size - 1)
        BasicStackString &operator+=(const BasicStackString<CharT, OtherSize> &other);
        BasicStackString &operator+=(const std::string_view &other);
        BasicStackString &operator+=(const char *other);

        [[nodiscard]] static size_t capacity();
        [[nodiscard]] size_t len() const;
        [[nodiscard]] size_t length() const;

        [[nodiscard]] std::string_view getStringView() const;
        [[nodiscard]] explicit operator std::string_view() const;

        bool set(const char *value);
        bool set(const char *value, size_t length);
        bool set(const std::string_view &view);
        template<size_t OtherLength>
        requires (OtherLength <= Size)
        bool set(const BasicStackString<CharT, OtherLength> &view);

        void clear();
        bool append(const char *str, size_t size);
        bool append(const char *str);

        bool append(std::string_view &view);
        template<size_t OtherStrSize>
        requires (OtherStrSize <= Size)
        bool append(const BasicStackString<CharT, OtherStrSize> &other);
    };

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size>::BasicStackString(const std::string_view &other) {
        set(other);
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size>::BasicStackString(const char *other) {
        set(other);
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::canHold(size_t length) {
        // For terminating zero char
        return length <= Size - 1;
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::canHold(const char *str) {
        return safe_strlen(str, Size) <= Size - 1;
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::canHold(const std::string_view &view) {
        return canHold(view.data());
    }

    template<typename CharT, size_t Size>
    template<size_t OtherSize, typename>
    requires (OtherSize <= Size)
    BasicStackString<CharT, Size> &BasicStackString<CharT, Size>::operator=(const BasicStackString<CharT, OtherSize> &other) {
        set(other);
        return *this;
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size> &BasicStackString<CharT, Size>::operator=(const std::string_view &other) {
        set(other.data(), other.size());
        return *this;
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size> &BasicStackString<CharT, Size>::operator=(const char *other) {
        set(other);
        return *this;
    }

    template<typename CharT, size_t Size>
    template<size_t OtherSize> requires (OtherSize <= Size - 1)
    BasicStackString<CharT, Size> &BasicStackString<CharT, Size>::operator+=(
        const BasicStackString<CharT, OtherSize> &other) {
        append(other);
        return *this;
    }

    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size> & BasicStackString<CharT, Size>::operator+=(const std::string_view &other) {
        append(other);
        return *this;
    }


    template<typename CharT, size_t Size>
    BasicStackString<CharT, Size> & BasicStackString<CharT, Size>::operator+=(const char *other) {
        append(other);
        return *this;
    }

    template<typename CharT, size_t Size>
    size_t BasicStackString<CharT, Size>::capacity() {
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

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::set(const char *value) {
        return set(value, safe_strlen(value, Size));
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::set(const char *value, size_t length) {
        if (!canHold(length)) {
            return false;
        }

        std::memcpy(this->data(), value, length);
        this->data()[length] = '\0';
        return true;
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::set(const std::string_view &view) {
        return set(view.data(), view.size());
    }

    template<typename CharT, size_t Size>
    template<size_t OtherLength> requires (OtherLength <= Size)
    bool BasicStackString<CharT, Size>::set(const BasicStackString<CharT, OtherLength> &view) {
        return set(view.data());
    }

    template<typename CharT, size_t Size>
    void BasicStackString<CharT, Size>::clear() {
        std::memset(this->data(), '\0', Size - 1);
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::append(const char *str, size_t size) {
        const auto currentSize = len();
        const auto stringLength = safe_strlen(str, Size);
        const auto newSize = currentSize + stringLength;

        if (newSize >= Size - 1) {
            return false;
        }

        std::memcpy(this->data() + currentSize, str, stringLength);
        this->data()[newSize] = '\0';

        return true;
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::append(const char *str) {
        return append(str, safe_strlen(str, Size));
    }

    template<typename CharT, size_t Size>
    bool BasicStackString<CharT, Size>::append(std::string_view &view) {
        return append(view.data());
    }

    template<typename CharT, size_t Size>
    template<size_t OtherStrSize> requires (OtherStrSize <= Size)
    bool BasicStackString<CharT, Size>::append(const BasicStackString<CharT, OtherStrSize> &other) {
        return append(other.data());
    }
}

template<size_t Size>
using BasicStackString = String::BasicStackString<char, Size>;