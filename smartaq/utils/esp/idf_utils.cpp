#include "esp_sntp.h"

#include "build_config.h"
#include "utils/logger.h"
#include "utils/esp/idf_utils.h"

#include <chrono>

SntpClock::SntpClock() {
    std::call_once(_init_flag, &SntpClock::init);
}

// TODO: add special snowflow method for esp
void SntpClock::init() {
    Logger::log(LogLevel::Info, "Initializing sntp");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(SntpClock::syncCallback);
    esp_sntp_init();
}

void SntpClock::syncCallback(timeval *val) {
}

void waitForClockSync(std::time_t *now, std::tm *timeinfo) {
    SntpClock clock{};

    std::tm local_timeinfo;
    std::time_t local_now;

    std::tm *dst_timeinfo = timeinfo != nullptr ? timeinfo : &local_timeinfo;
    std::time_t *dst_now = now != nullptr ? now : &local_now;

    do {
        time(dst_now);
        localtime_r(dst_now, dst_timeinfo);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (dst_timeinfo->tm_year < 100);
}