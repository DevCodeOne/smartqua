#pragma once

#include <array>
#include <shared_mutex>
#include <mutex>
#include <cstdarg>
#include <cstdio>

#include "esp_log.h"

#include "utils/thread_utils.h"
#include "smartqua_config.h"

class sd_card_logger final {
    public:
        static bool install_sd_card_logger();
        static bool uninstall_sd_card_logger();

        static inline constexpr char log_path_format [] = "%s/logs";
        // 1/2 MB
        static inline constexpr size_t MAX_LOG_FILESIZE = static_cast<size_t>(1024ul * 1024ul / 2);
    private:
        static int printf_log(const char *fmt, va_list list);
        static FILE *open_current_log();
        static void potential_flush(int newly_written);

        static inline std::shared_mutex _instance_mutex;
        static inline vprintf_like_t _original_function = nullptr;
        static inline FILE *_log_output_file = nullptr;
        static inline std::size_t _since_last_flush = 0;
};