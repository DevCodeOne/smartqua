#pragma once

#include <array>
#include <mutex>
#include <string_view>
#include <tuple>
#include <cstdarg>
#include <cstdio>
#include <optional>

#include "esp_log.h"

#include "utils/thread_utils.h"
#include "storage/rest_storage.h"
#include "utils/utils.h"

enum struct LogLevel {
    Debug, Info, Warning, Error
};

template<typename ... Sinks>
class ApplicationLogger final {
    public:
        ApplicationLogger() = delete;
        ~ApplicationLogger() = delete;

        static void install();

        template<typename ... Arguments>
        static bool log(LogLevel level, const char *fmt, Arguments &&... args);

        static void ignoreLogsBelow(LogLevel level);

        static int printf_log(const char *fmt, va_list list);
    private:
        static void initSinksAndInstall();

        static inline std::tuple<Sinks ...> _sinks;
        static inline std::once_flag _initializedSinks;
        static inline std::mutex _sinkMutex;
        static inline vprintf_like_t _originalFunction;
        static inline LogLevel _ignoreLogsBelow;
        static inline DoFinally unistallHook{ []() {
                std::apply([](auto && ...currentSink) {
                    (currentSink.uninstall(), ...);

                    esp_log_set_vprintf(_originalFunction);
                }, _sinks);
            }
        };
};

template<typename ... Sinks>
void ApplicationLogger<Sinks ...>::initSinksAndInstall() {
    std::call_once(_initializedSinks, []() {
        std::apply([](auto && ...currentSink) {
            (currentSink.install(), ...);
        }, _sinks);

        const auto previous_function = esp_log_set_vprintf(ApplicationLogger::printf_log);

        if (_originalFunction == nullptr) {
            _originalFunction = previous_function;
        }
    });
}

template<typename ... Sinks>
void ApplicationLogger<Sinks ...>::install() {
    initSinksAndInstall();
}


template<typename ... Sinks>
void ApplicationLogger<Sinks ...>::ignoreLogsBelow(LogLevel level) {
    std::unique_lock sinkGuard{_sinkMutex};

    _ignoreLogsBelow = level;
}

template<typename ... Sinks>
template<typename ... Arguments>
bool ApplicationLogger<Sinks ...>::log(LogLevel level, const char *fmt, Arguments &&... args) {
    std::unique_lock sinkGuard{_sinkMutex};

    const auto loggedSuccessfully = std::apply([&](auto && ... currentSink) {
        std::array results{(currentSink.log(level, fmt, args ...), ...)};

        esp_log_write(esp_log_level_t::ESP_LOG_INFO, "ApplicationLogger", fmt, std::forward<Arguments>(args) ...);
        esp_log_write(esp_log_level_t::ESP_LOG_INFO, "ApplicationLogger", "\n");

        for (const auto currentResult : results) {
            if (!currentResult) {
                return false;
            }
        }

        return true;
    }, _sinks);

    return loggedSuccessfully;
}

class HttpLogSink final {
    public:
        bool install();
        bool uninstall();

        template<size_t DstSize>
        static bool generateRestTarget(std::array<char, DstSize> &dst);
        
        template<typename ... Arguments>
        bool log(LogLevel level, const char *fmt, Arguments && ... args);
    private:
        // static std::optional<RestStorage<HttpLogSink, RestDataType::Text>> logSink;
};

class SdCardSink final {
    public:
        static inline constexpr char log_path_format [] = "%s/logs";
        static inline constexpr size_t MAX_LOG_FILESIZE = static_cast<size_t>(1024ul * 1024ul / 2); // 0.5 MB

        bool install();
        bool uninstall();
 
        template<typename ... Arguments>
        bool log(LogLevel level, const char *fmt, Arguments && ... args);
    private:
        FILE *open_current_log();
        void potential_flush(int newly_written);

        FILE *_log_output_file = nullptr;
        std::size_t _since_last_flush = 0;
};

template<typename ... Arguments>
bool HttpLogSink::log(LogLevel level, const char *fmt, Arguments && ... args) {
    return true;
}

template<typename ... Arguments>
bool SdCardSink::log(LogLevel level, const char *fmt, Arguments && ... args) {
    auto current_log_file = open_current_log();

    if (current_log_file == nullptr) {
        return false;
    }

    const auto written = fprintf(_log_output_file, fmt, std::forward<Arguments>(args) ...);

    potential_flush(written);

    return true; 

}