#pragma once

#include <chrono>
#include <cmath>

enum struct WeekDay : uint8_t {
    sunday = 0, monday = 1, tuesday = 2, wednesday = 3, thursday = 4, friday = 5, saturday = 6
};

enum struct DaySearchDirection {
    Previous, Next
};

WeekDay getDayOfWeek(const std::tm &timeInfo);
WeekDay getDayOfWeek();

WeekDay getPreviousDay(WeekDay ofThisDay);
WeekDay getNextDay(WeekDay ofThisDay);
WeekDay getDayInDirection(WeekDay ofThisDay, DaySearchDirection direction);

std::tm currentTime();
bool validateTime(const std::tm &timeInfo);

template<typename T, typename U>
struct IsLargerOrEqualDuration {
    static constexpr bool value = std::ratio_less_equal_v<typename T::period, typename U::period>;
};

template<typename T, typename U = std::chrono::seconds>
concept LargerOrEqualDuration = IsLargerOrEqualDuration<T, U>::value;

template<typename T>
concept SecondOrLargerDuration = LargerOrEqualDuration<T, std::chrono::seconds>;

template<SecondOrLargerDuration DurationType>
DurationType getTimeOfDay(const std::tm &timeInfo) {
    using namespace std::chrono;
    return DurationType{hours{timeInfo.tm_hour}
                        + minutes{timeInfo.tm_min}
                        + seconds{timeInfo.tm_sec}
    };
}

template<typename DurationType>
DurationType getTimeOfDay() {
    const auto timeInfo = currentTime();
    return getTimeOfDay<DurationType>(timeInfo);
}

template<typename DurationType>
DurationType sinceWeekBeginning(const std::tm &timeInfo) {
    using namespace std::chrono;

    return getTimeOfDay<DurationType>(timeInfo) + duration_cast<DurationType>(days{timeInfo.tm_wday});
};

template<typename DurationType>
DurationType sinceWeekBeginning() {
    return getTimeOfDay<DurationType>(currentTime());
}

template<typename DurationType>
requires (LargerOrEqualDuration<DurationType, std::chrono::hours>)
DurationType diffWithDurationSinceWeekBeginning(const DurationType &end, const DurationType &start) {
    using namespace std::chrono;
    auto durationInWeek = duration_cast<DurationType>(days{7});
    auto diff = end - start;
    if (diff <= DurationType{0}) {
        const auto offset = std::ceil(std::fabs(diff.count() / static_cast<float>(durationInWeek.count())));
        diff += duration_cast<DurationType>(durationInWeek * offset);
    }

    return diff;
}