#pragma once

#include <type_traits>

#include "utils/type/unique_type_list.h"
#include "utils/type/enum_sequence.h"
#include "utils/constexpr_for.h"

template<auto v, typename T>
requires (IsEnumType<decltype(v)>)
struct EnumTypePair {
    using type = T;
    static constexpr auto value = v;
};

template<typename ... ValueTypePairs>
requires (AllUniqueV<std::integral_constant<decltype(ValueTypePairs::value), ValueTypePairs::value> ...>)
class EnumTypeMap {
public:
    using EnumType = std::common_type_t<std::decay_t<decltype(ValueTypePairs::value)> ...>;
    using KeyList = UniqueTypeList<std::integral_constant<std::decay_t<decltype(ValueTypePairs::value)>,
        ValueTypePairs::value> ...>;
    using KeyListSequence = EnumSequence<EnumType, ValueTypePairs::value ...>;

    using KeyValueList = UniqueTypeList<ValueTypePairs ...>;

    template<auto EnumValue>
    static constexpr auto IndexOfEnumValue = KeyList::template IndexOf<std::integral_constant<std::decay_t<decltype(EnumValue)>, EnumValue>>;

    template<auto EnumValue>
    using map = KeyValueList::template TypeAt<IndexOfEnumValue<EnumValue>>::type;

    static constexpr size_t Size = sizeof...(ValueTypePairs);

    template <typename T,
              auto ArraySize = CountTypeV<T, typename ValueTypePairs::type...>>
    static constexpr std::array<EnumType, ArraySize> collectKeysWithType()
    {
        std::array<EnumType, ArraySize> keys{};
        using ListOfEntries = std::tuple<ValueTypePairs...>;

        auto currentEntry = keys.begin();

        constexprFor(ListOfEntries{},
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