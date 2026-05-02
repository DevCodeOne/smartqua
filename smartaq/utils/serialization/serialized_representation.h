#pragma once

#include <array>

#include "utils/stack_string.h"
#include "utils/serialization/serializer.h"

template<typename T>
concept ValidBaseType = requires(T a) {
    { T::StorageName } -> std::convertible_to<const char *>;
} && std::is_trivial_v<T>;

template<ValidBaseType BaseType>
struct BinarySerializedRepresentation
{
    static const constexpr char *const StorageName{ BaseType::StorageName };

    bool shouldBeWritten(const BinarySerializedRepresentation &other) const
    {
        if (!initialized && !other.initialized)
        {
            return false;
        }

        if (!other.initialized)
        {
            return true;
        }

        if (name != other.name)
        {
            return true;
        }

        return std::memcmp(&value, &other.value, sizeof(value)) != 0;
    }

    BaseType value;
    bool initialized;
    BasicStackString<name_length> name;
};

// TODO: separate name_length
template<ValidBaseType BaseType, size_t Size>
    struct BinarySerializedRepresentationCollection {
    using ValueType = BinarySerializedRepresentation<BaseType>;

    static const constexpr char *const StorageName{ BaseType::StorageName };

    auto begin() { return values.begin(); }
    auto end() { return values.end(); }

    auto begin() const { return values.begin(); }
    auto end() const { return values.end(); }

    std::array<BinarySerializedRepresentation<BaseType>, Size> values;
};

// TODO: replace const char * -> name.data() with better type
template<ValidBaseType T, size_t Size>
struct Serializer<BinarySerializedRepresentationCollection<T, Size>>
{
    template<typename Callback>
    static void serialize(const BinarySerializedRepresentationCollection<T, Size> &writtenValue, const BinarySerializedRepresentationCollection<T, Size> &value, Callback callback)
    {
        char buffer[16];
        for (const auto &[index, values] : std::views::enumerate(std::views::zip(writtenValue, value)))
        {
            if (auto &[written, current] = values; written.shouldBeWritten(current))
            {
                snprintf(buffer, sizeof(buffer), "%u.bin", index);
                callback(buffer, current);
            }
        }
    }

    template<typename Callback>
    static void deserialize(BinarySerializedRepresentationCollection<T, Size> &value, Callback callback)
    {
        Logger::log(LogLevel::Debug, "Deserializing collection");
        char buffer[16];
        for (unsigned int index = 0; index < Size; ++index)
        {
            snprintf(buffer, sizeof(buffer), "%u.bin", index);
            Logger::log(LogLevel::Debug, "Deserializing item %u", index);
            callback(buffer, value.values[index]);
        }
    }
};

