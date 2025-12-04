#pragma once

#include <array>
#include <bit>
#include <optional>

template<typename Destination, typename ... IntTypes>
constexpr std::optional<Destination> setFlagsAtPos(IntTypes ... Arguments) {
    [[maybe_unused]] static constexpr auto BitWidthDestination = sizeof(Destination) * CHAR_BIT;

    Destination combinedFlags = 0;

    std::array flags{ Arguments ... };

    for (size_t i = 0; i < sizeof...(IntTypes); i++) {
        if (flags[i] < 0 || flags[i] >= BitWidthDestination) {
            return {};
        }

        combinedFlags |= 1 << flags[i];
    }

    return { combinedFlags };
}

template<typename Destination, auto ... IntTypes>
static constexpr Destination CombinedFlagsAtPos = *setFlagsAtPos<Destination>(IntTypes ...);