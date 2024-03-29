#pragma once

#include <mutex>

class timeval;

class sntp_clock final {
    public:
        sntp_clock();
        ~sntp_clock() = default;
    private:
        static void init_sntp();
        static void sync_callback(timeval *);

        static inline std::once_flag _init_flag;
};

void wait_for_clock_sync(std::time_t *now = nullptr, std::tm *timeinfo = nullptr);