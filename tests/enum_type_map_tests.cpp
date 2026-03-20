#include "utils/type/enum_type_map.h"

#include "gtest/gtest.h"

enum TestEnum
{
    One, Two, Three, Four
};

using TestMap = EnumTypeMap<EnumTypePair<TestEnum::One, int>, EnumTypePair<TestEnum::Two, float>, EnumTypePair<
                                TestEnum::Three, bool>, EnumTypePair<TestEnum::Four, int>>;

TEST(EnumTypeMapTests, collectAllKeysWithTypeTests)
{
    constexpr auto result = TestMap::collectKeysWithType<int>();

    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], TestEnum::One);
    EXPECT_EQ(result[1], TestEnum::Four);
}
