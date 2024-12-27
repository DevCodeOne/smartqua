#pragma once

#include <chrono>

enum struct weekday : uint32_t {
    sunday = 0, monday = 1, tuesday = 2, wednesday = 3, thursday = 4, friday = 5, saturday = 6
};

weekday getDayOfWeek(const std::tm &timeInfo);
weekday getDayOfWeek();

weekday getPreviousDay(weekday ofThisDay);
weekday getNextDay(weekday ofThisDay);

std::tm currentTime();

// TODO: don't allow smaller units than usec
template<typename DurationType>
DurationType getTimeOfDay(const std::tm &timeinfo) {
    using namespace std::chrono;
    return duration_cast<DurationType>(hours{timeinfo.tm_hour} + minutes{timeinfo.tm_min} + seconds{timeinfo.tm_sec}
    );
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
};