#pragma once

#include <cstdio>

#include <esp_mac.h>
#include <esp_log_write.h>

#include "network/network_info.h"
#include "utils/logger.h"
#include "utils/filesystem_utils.h"

class EspIdfBackend {
public:
    static void uninstall() {
        //esp_log_set_vprintf(_originalFunction);
    }

    static void install() {
        //const auto previous_function = esp_log_set_vprintf(ApplicationLogger::printf_log);

        //if (_originalFunction == nullptr) {
        //    _originalFunction = previous_function;
        //}
    }

    template<typename ... Arguments>
    static void log(LogLevel level, const char *fmt, Arguments &&... args) {
        esp_log_write(esp_log_level_t::ESP_LOG_INFO, "ApplicationLogger", fmt, std::forward<Arguments>(args) ...);
        esp_log_write(esp_log_level_t::ESP_LOG_INFO, "ApplicationLogger", "\n");
    }
private:
    static inline vprintf_like_t _originalFunction;
};