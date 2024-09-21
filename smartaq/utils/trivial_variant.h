#pragma once

#include <array>

// TOOD: Check if all types trivial
template<typename ... Types>
struct TrivialVariant {
    static constexpr size_t MaxTypeSize = 64;

    template<typename CheckType>
    bool isType() {
        return false;
    }

    template<typename T>
    const T *asType() const {
        return nullptr;
    }

    template<typename T>
    void setValue(const T &value) {

    }

    int currentIndex;
    std::array<char, MaxTypeSize> valueStorage;
};