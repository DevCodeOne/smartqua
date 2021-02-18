#include "utils/idf-utils.h"

#include "esp_sntp.h"
#include "esp_log.h"

sntp_clock::sntp_clock() {
    std::call_once(_init_flag, &sntp_clock::init_sntp);
}

void sntp_clock::init_sntp() {
    ESP_LOGI("sntp_clock", "Initializing sntp");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_clock::sync_callback);
    sntp_init();
}

void sntp_clock::sync_callback(timeval *val) { 
    ESP_LOGI("sntp_clock", "sntp sync callback");
}