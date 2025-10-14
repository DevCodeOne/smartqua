#pragma once

#include <variant>

template<typename T, typename ... VariantArgs>
std::optional<T> getOpt(const std::variant<VariantArgs ...> &variant)
{
    if (!std::holds_alternative<T>(variant))
    {
        return {};
    }

    return { std::get<T>(variant) };
}

// TODO: Make sure one is contained in the other
template<typename... V1Args, typename... V2Args>
void convertToVariant(const std::variant<V1Args...>& v1, std::variant<V2Args...>& v2)
{
    std::visit([&v2](const auto& containedValue)
    {
        return v2 = containedValue;
    }, v1);
}
