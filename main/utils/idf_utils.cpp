#include "utils/idf_utils.h"

#include "esp_sntp.h"

#include "utils/logger.h"
#include "smartqua_config.h"

sntp_clock::sntp_clock() {
    std::call_once(_init_flag, &sntp_clock::init_sntp);
}

void sntp_clock::init_sntp() {
    Logger::log(LogLevel::Info, "Initializing sntp");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_clock::sync_callback);
    sntp_init();
}

void sntp_clock::sync_callback(timeval *val) { 
}

void wait_for_clock_sync(std::time_t *now, std::tm *timeinfo) {
    sntp_clock clock{};

    std::tm local_timeinfo;
    std::time_t local_now;

    std::tm *dst_timeinfo = timeinfo != nullptr ? timeinfo : &local_timeinfo;
    std::time_t *dst_now = now != nullptr ? now : &local_now;

    do {
        time(dst_now);
        localtime_r(dst_now, dst_timeinfo);
    } while (dst_timeinfo->tm_year < 100);
}