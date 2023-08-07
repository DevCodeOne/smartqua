#pragma once

#include <mutex>

#include "esp_heap_caps.h"

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

template<typename T>
struct SPIRAMDeleter {
    void operator()(T *value) const noexcept {
        if (value) {
            heap_caps_free(value);
        }
    }
};

void wait_for_clock_sync(std::time_t *now = nullptr, std::tm *timeinfo = nullptr);