#pragma once

#include <optional>
#include <charconv>

template<typename T>
concept CharRange = requires (T t)
{
    { t.begin() } -> std::same_as<const char *>;
    { t.end() } -> std::same_as<const char *>;
};

template<typename T, CharRange Input>
std::optional<T> convertFromCharRange(const Input &rangeInput)
{
    T dest;
    if (std::from_chars(rangeInput.begin(), rangeInput.end(), dest).ec != std::errc())
    {
        return {};
    }

    return { dest };
}