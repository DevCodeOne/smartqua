#include <gtest/gtest.h>

#include "drivers/device_state.h"

enum struct StateEnum
{
    One, Two, Three
};

using StateMap = EnumTypeMap<
    EnumTypePair<StateEnum::One, int>,
    EnumTypePair<StateEnum::Two, float>,
    EnumTypePair<StateEnum::Three, bool>>;

using GlobalState = DeviceState<StateEnum, StateMap>;

TEST(DeviceState, Basic)
{
    GlobalState  state;
    state.setValue<StateEnum::One>(42);
    state.setValue<StateEnum::Two>(42.42f);
    state.setValue<StateEnum::Three>(true);

    EXPECT_TRUE((std::is_same_v<GlobalState::TupleType, std::tuple<int, float, bool>>));

    EXPECT_EQ(state.getValue<StateEnum::One>(), 42);
    EXPECT_FLOAT_EQ(state.getValue<StateEnum::Two>(), 42.42f);
    EXPECT_EQ(state.getValue<StateEnum::Three>(), true);

    EXPECT_TRUE((std::is_same_v<decltype(state.getValue<StateEnum::One>()), int>));
    EXPECT_TRUE((std::is_same_v<decltype(state.getValue<StateEnum::Two>()), float>));
    EXPECT_TRUE((std::is_same_v<decltype(state.getValue<StateEnum::Three>()), bool>));
}

struct WithDeviceState { using State = GlobalState; };

struct WithDeviceStateWrongType { using State = int; };

struct WithoutDeviceState { };

TEST(DeviceState, HasDeviceState)
{
    EXPECT_TRUE((HasDeviceState<WithDeviceState>::value));
    EXPECT_FALSE((HasDeviceState<WithDeviceStateWrongType>::value));
    EXPECT_FALSE((HasDeviceState<WithoutDeviceState>::value));
}

