#include <gtest/gtest.h>

#include <list>
#include <vector>

#include "utils/type/type_utils.h"

TEST(IsInstantiationOf, WhenInstantiationOfReturnsTrue)
{
    EXPECT_TRUE((IsInstantiationOf<std::vector, std::vector<int>>::value)) <<
 "std::vector<int> should be an instantiation of std::vector";
    EXPECT_TRUE((!IsInstantiationOf<std::vector, std::list<int>>::value)) <<
 "std::list<int> should not be an instantiation of std::vector";
}
