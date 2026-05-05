#pragma once

#include <algorithm>
#include <type_traits>

namespace linear_alg
{
template<typename OutputType, typename InputType, typename StartType, typename EndType>
constexpr OutputType linearInterpolate(
    InputType input,
    InputType inputStart,
    InputType inputEnd,
    StartType outputStart,
    EndType outputEnd)
{
    if (inputEnd == inputStart)
    {
        return static_cast<OutputType>(outputStart);
    }

    return static_cast<OutputType>(
        static_cast<double>(outputStart) +
        ((static_cast<double>(outputEnd) - static_cast<double>(outputStart))
            * (static_cast<double>(input) - static_cast<double>(inputStart)))
        / (static_cast<double>(inputEnd) - static_cast<double>(inputStart)));
}

template<typename OutputType, typename InputType, typename StartType, typename EndType>
constexpr OutputType linearMap(
    InputType input,
    InputType inputStart,
    InputType inputEnd,
    StartType outputStart,
    EndType outputEnd,
    bool clampInput = true)
{
    if (clampInput)
    {
        if (inputStart < inputEnd)
        {
            input = std::clamp(input, inputStart, inputEnd);
        }
        else
        {
            input = std::clamp(input, inputEnd, inputStart);
        }
    }

    return linearInterpolate<OutputType>(input, inputStart, inputEnd, outputStart, outputEnd);
}
}
