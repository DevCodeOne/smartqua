#pragma once

#include "esp_heap_caps.h"

#include <cstdio>

enum struct DeviceKind {
    ESPDevice
};

template<DeviceKind Kind>
class SystemInfo;

template<>
class SystemInfo<DeviceKind::ESPDevice> {
    public:
        static inline int printSystemHealthToString(char *out, size_t len);
        static inline int printHeapInfoToString(char *out, size_t len);
    private:
};

inline int SystemInfo<DeviceKind::ESPDevice>::printSystemHealthToString(char *out, size_t len) {
    char *startForNextOutput = out;
    if (auto bytesWritten = printHeapInfoToString(startForNextOutput, len); bytesWritten < len && bytesWritten >= 0) {
        startForNextOutput += bytesWritten;
    } else {
        return -1;
    }

    return startForNextOutput - out;
}

inline int SystemInfo<DeviceKind::ESPDevice>::printHeapInfoToString(char *out, size_t len) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_8BIT);

    return std::snprintf(out, len, 
                            "[Begin Report Ram]\n"
                            "Free bytes 8bit addressable    : %10u \n"
                            "Allocated bytes                : %10u\n"
                            "[End Report Ram]\n",
                            static_cast<unsigned int>(info.total_free_bytes),
                            static_cast<unsigned int>(info.total_allocated_bytes)
                            );
}

using CurrentSystem = SystemInfo<DeviceKind::ESPDevice>;