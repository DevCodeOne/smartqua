#pragma once

#include <chrono>
#include <sys/time.h>

enum struct weekday : uint32_t {
    monday = 0, tuesday = 1, wednesday = 2, thursday = 3, friday = 4, saturday = 5, sunday = 6
};

weekday getDayOfWeek();
weekday getPreviousDay(weekday ofThisDay);
weekday getNextDay(weekday ofThisDay);

template<typename DurationType>
DurationType getTimeOfDay() {
    timeval timeOfDay;

    gettimeofday(&timeOfDay, nullptr);

    return std::chrono::duration_cast<DurationType>( std::chrono::microseconds{ timeOfDay.tv_usec } );

}