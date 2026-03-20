#include <gtest/gtest.h>

#include "utils/type/type_list.h"

template<typename T, typename TypeList>
struct AllFilter
{
    static constexpr bool value = false;
};

template<typename T, typename TypeList>
struct OnlyInts
{
    static constexpr bool value = std::is_integral_v<T>;
};

TEST(TypeListGenerator, Basic)
{
    EXPECT_TRUE((std::is_same_v<TypeListGenerator<AllFilter, int, float, double>::type, TypeList<>>));
    EXPECT_TRUE((std::is_same_v<TypeListGenerator<OnlyInts, int, float, double>::type, TypeList<int>>));
    EXPECT_TRUE((std::is_same_v<TypeListGenerator<UniqueTypes, int, int, float, int, double>::type, TypeList<int, float, double>>));

    using OnlyOneInt = TypeListGenerator<
            CombineFilters<
                UniqueTypes,
                OnlyInts>::GenerateFilter,
            int, float, double, int>::type;
    EXPECT_TRUE((std::is_same_v<OnlyOneInt, TypeList<int>>));
}