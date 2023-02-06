#pragma once

#include <chrono>
#include <sys/time.h>

enum struct weekday : uint32_t {
    sunday = 0, monday = 1, tuesday = 2, wednesday = 3, thursday = 4, friday = 5, saturday = 6
};

weekday getDayOfWeek();
weekday getPreviousDay(weekday ofThisDay);
weekday getNextDay(weekday ofThisDay);

// TODO: don't allow smaller units than usec
template<typename DurationType>
DurationType getTimeOfDay(const std::tm &timeinfo) {
    return std::chrono::duration_cast<DurationType>(
        std::chrono::hours{timeinfo.tm_hour} + std::chrono::minutes{timeinfo.tm_min} + std::chrono::seconds{timeinfo.tm_sec}
    );

}

template<typename DurationType>
DurationType getTimeOfDay() {
    std::time_t now;
    std::tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return getTimeOfDay<DurationType>(timeinfo);
}

template<typename DurationType>
DurationType sinceYearBeginning() {
    std::time_t now;
    std::tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return std::chrono::duration_cast<DurationType>(
        getTimeOfDay<DurationType>(timeinfo) + std::chrono::days{timeinfo.tm_yday}
    );
}

template<typename DurationType>
DurationType sinceWeekBeginning() {
    std::time_t now;
    std::tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return getTimeOfDay<DurationType>(timeinfo) + std::chrono::duration_cast<DurationType>(std::chrono::days{timeinfo.tm_wday});
};