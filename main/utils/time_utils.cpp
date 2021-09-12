#include "time_utils.h"

#include <type_traits>

weekday getDayOfWeek() {
    std::time_t now;
    std::tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return static_cast<weekday>(timeinfo.tm_wday);
}

weekday getPreviousDay(weekday ofThisDay) {
    if (ofThisDay == weekday::sunday) {
        return weekday::saturday;
    }

    return static_cast<weekday>(static_cast<std::underlying_type_t<weekday>>(ofThisDay) - 1);
}

weekday getNextDay(weekday ofThisDay) {
    if (ofThisDay == weekday::saturday) {
        return weekday::sunday;
    }

    return static_cast<weekday>(static_cast<std::underlying_type_t<weekday>>(ofThisDay) + 1);
}