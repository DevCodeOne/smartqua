#pragma once

#include <algorithm>
#include <type_traits>

#include "utils/utils.h"
#include "utils/constexpr_for.h"

template<typename T>
concept EnumType = std::is_enum_v<T>;

template<auto v, typename T>
requires (EnumType<decltype(v)>)
struct EnumTypePair {
    using type = T;
    static constexpr auto value = v;
};


template<typename ... ValueTypePairs>
requires (AllUniqueV<std::integral_constant<decltype(ValueTypePairs::value), ValueTypePairs::value> ...>)
class EnumTypeMap {
public:
    using KeyList = UniqueTypeList<std::integral_constant<std::decay_t<decltype(ValueTypePairs::value)>,
        ValueTypePairs::value> ...>;
    using KeyValueList = UniqueTypeList<ValueTypePairs ...>;

    template<auto EnumValue>
    static constexpr auto IndexOfEnumValue = KeyList::template IndexOf<std::integral_constant<std::decay_t<decltype(EnumValue)>, EnumValue>>;

    template<auto EnumValue>
    using map = KeyValueList::template TypeAt<IndexOfEnumValue<EnumValue>>::type;

    static constexpr size_t Size = sizeof...(ValueTypePairs);

    template <typename T, typename ValueType = std::common_type_t<decltype(ValueTypePairs::value)...>,
              auto ArraySize = CountTypeV<T, typename ValueTypePairs::type...>>
    static constexpr std::array<ValueType, ArraySize> collectKeysWithType()
    {
        std::array<ValueType, ArraySize> keys{};
        using ListOfEntries = std::tuple<ValueTypePairs...>;

        auto currentEntry = keys.begin();

        ConstexprFor<Size - 1>::doCall(ListOfEntries{},
                                       [&]<auto V, typename InnerType>(const EnumTypePair<V, InnerType>&)
                                       {
                                           if (std::is_same_v<InnerType, T>)
                                           {
                                               *currentEntry = V;
                                               ++currentEntry;
                                           }
                                       });

        return keys;
    }
};