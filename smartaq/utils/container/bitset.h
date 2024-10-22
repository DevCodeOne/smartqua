#pragma once

#include <cstdint>
#include <type_traits>

template<auto Bits>
using ChooseInteger =
std::conditional_t<Bits <= 8, uint8_t,
    std::conditional_t<Bits <= 16, uint16_t,
        std::conditional_t<Bits <= 32, uint32_t,
            std::conditional_t<Bits <= 64, uint64_t, void>>>>;

template<uint8_t Length>
requires (!std::is_same_v<ChooseInteger<Length>, void>)
struct Bitset {
    using UsedInt = ChooseInteger<Length>;

    constexpr void set(uint8_t bit, bool value) {
        const auto clearMask = (~(UsedInt(1) << bit));
        const auto setMask = (value & UsedInt(0b1)) << bit;
        data = (data & clearMask) | setMask;
    };

    constexpr void invert(uint8_t bit) {
        set(bit, !test(bit));
    }

    constexpr bool test(uint8_t bit) {
        return data & (UsedInt(1) << bit);
    }

    UsedInt data{0};
};
