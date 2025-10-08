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

template<typename T, typename ... Args>
[[nodiscard]] auto makeUniquePtrLargeType(Args && ... args) {
#ifdef USE_PSRAM
    void *data = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return makePointerAt<std::unique_ptr, T>(data, SPIRAMDeleter<T>{}, std::forward<Args>(args) ...);
#else
    static_assert(false, "PSRAM not enabled, cannot allocate large types");
#endif
}

