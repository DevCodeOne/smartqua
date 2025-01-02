#include <type_traits>

#include "utils/time/time_utils.h"
#include "utils/check_assign.h"

weekday getDayOfWeek() {
    return getDayOfWeek(currentTime());
}

weekday getDayOfWeek(const std::tm &day) {
    return static_cast<weekday>(day.tm_wday);
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

std::tm currentTime() {
    std::time_t now;
    std::tm timeinfo{};

    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

static constexpr int YEAR_MIN = 70;
static constexpr int MONTH_MIN = 0;
static constexpr int MONTH_MAX = 11;
static constexpr int DAY_MIN = 1;
static constexpr int DAY_MAX = 31;
static constexpr int HOUR_MIN = 0;
static constexpr int HOUR_MAX = 23;
static constexpr int MINUTE_MIN = 0;
static constexpr int MINUTE_MAX = 59;
static constexpr int SECOND_MIN = 0;
static constexpr int SECOND_MAX = 59;

bool validateTime(const std::tm &timeInfo) {
    return isInRange(timeInfo.tm_year, YEAR_MIN, std::numeric_limits<int>::max()) &&
           isInRange(timeInfo.tm_mon, MONTH_MIN, MONTH_MAX) &&
           isInRange(timeInfo.tm_mday, DAY_MIN, DAY_MAX) &&
           isInRange(timeInfo.tm_hour, HOUR_MIN, HOUR_MAX) &&
           isInRange(timeInfo.tm_min, MINUTE_MIN, MINUTE_MAX) &&
           isInRange(timeInfo.tm_sec, SECOND_MIN, SECOND_MAX);
}