#pragma once

#include <array>

#include "utils/type/type_helper.h"

template<IsByteCopyable ... Types>
struct TrivialVariant {
    static constexpr size_t MaxTypeSize = std::ranges::max(std::array{ sizeof(Types) ... });

    template<typename CheckType>
    bool isType() {
        return false;
    }

    template<typename T>
    requires (std::is_same_v<T, Types> || ...)
    const T *asType() const {
        return nullptr;
    }

    template<typename T>
    requires (std::is_same_v<T, Types> || ...)
    void setValue(const T &value) {

    }

    int currentIndex;
    std::array<char, MaxTypeSize> valueStorage;
};