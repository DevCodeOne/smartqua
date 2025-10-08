#pragma once

#include <mutex>

#include "utils/memory_helper.h"

#include "esp_heap_caps.h"

class timeval;

class SntpClock final {
    public:
        SntpClock();
        ~SntpClock() = default;
    private:
        static void init();
        static void syncCallback(timeval *);

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

void waitForClockSync(std::time_t *now = nullptr, std::tm *timeinfo = nullptr);

template<typename T, typename ... Args>
[[nodiscard]] auto makeUniquePtrLargeType(Args && ... args) {
#ifdef USE_PSRAM
    void *data = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return makePointerAt<std::unique_ptr, T>(data, SPIRAMDeleter<T>{}, std::forward<Args>(args) ...);
#else
    static_assert(false, "PSRAM not enabled, cannot allocate large types");
#endif
}

