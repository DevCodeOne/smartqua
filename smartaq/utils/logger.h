#pragma once

#include <array>
#include <mutex>
#include <string_view>
#include <tuple>
#include <cstdarg>
#include <cstdio>
#include <optional>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "frozen.h"

#include "network/network_info.h"
#include "storage/rest_storage.h"
#include "utils/filesystem_utils.h"
#include "utils/esp/thread_utils.h"

enum struct LogLevel {
    Debug, Info, Warning, Error
};

const char *to_string(LogLevel level);

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

                    //esp_log_set_vprintf(_originalFunction);
                }, _sinks);
            }
        };
};

template<typename ... Sinks>
void ApplicationLogger<Sinks ...>::initSinksAndInstall() {
    std::unique_lock sinkGuard{_sinkMutex};
    std::call_once(_initializedSinks, []() {
        std::apply([](auto && ...currentSink) {
            (currentSink.install(), ...);
        }, _sinks);

        //const auto previous_function = esp_log_set_vprintf(ApplicationLogger::printf_log);

        //if (_originalFunction == nullptr) {
        //    _originalFunction = previous_function;
        //}
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

template<ConstexprPath RemoteSettingPath>
struct HttpLogSinkPathGenerator {
    static constexpr const char *Path = RemoteSettingPath.value;

    template<size_t DstSize>
    static bool generateRestTarget(std::array<char, DstSize> &dst);

};

class VoidSink final {
    public:
        bool install() { return true; }
        bool uninstall() { return true; }

        
        template<typename ... Arguments>
        bool log(LogLevel level, const char *fmt, Arguments && ... args) { return true; }
    private:
};

template<ConstexprPath RemoteSettingPath>
class HttpLogSink final {
    public:
        bool install();
        bool uninstall();

        
        template<typename ... Arguments>
        bool log(LogLevel level, const char *fmt, Arguments && ... args);
    private:
        static inline RestStorage<HttpLogSinkPathGenerator<RemoteSettingPath>, RestDataType::Json> logSink;
};

/*
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
*/

template<ConstexprPath RemoteSettingPath>
template<size_t DstSize>
bool HttpLogSinkPathGenerator<RemoteSettingPath>::generateRestTarget(std::array<char, DstSize> &dst) {
    snprintf(dst.data(), dst.size(), "http://%s/log", Path);

    // TODO: check return value
    return true;
}

template<ConstexprPath RemoteSettingPath>
bool HttpLogSink<RemoteSettingPath>::install() { return true; }

template<ConstexprPath RemoteSettingPath>
bool HttpLogSink<RemoteSettingPath>::uninstall() { return true; }

template<ConstexprPath RemoteSettingPath>
template<typename ... Arguments>
bool HttpLogSink<RemoteSettingPath>::log(LogLevel level, const char *fmt, Arguments && ... args) {
    if (!NetworkInfo::canUseNetwork()) {
        return false;
    }

    std::once_flag initNameAndSink;
    static std::array<uint8_t, 6> mac;

    std::call_once(initNameAndSink, []() {
        esp_efuse_mac_get_default(mac.data());
    });

    std::array<char, 256> json_buf{0};

    const auto written = snprintf(json_buf.data(), json_buf.size(), R"({ "device_id" : "%02x-%02x-%02x-%02x-%02x-%02x", "level" : "%s", "msg" : ")", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            to_string(level)
    );

    // 3 bytes for closing `" }` and \0 smallest message is empty string
    if (written < 0 || written >= json_buf.size() - 4) {
        ESP_LOGI("HttpLogSink", "Couldn't write initial json message");
        return false;
    }

    auto complete_length = written;
    complete_length += snprintf(json_buf.data() + written, json_buf.size() - written, fmt, args ...);

    if (complete_length < written || complete_length >= json_buf.size() - 4) {
        ESP_LOGI("HttpLogSink", "Couldn't write log message");
        return false;
    }

    auto close_json = complete_length;
    close_json += snprintf(json_buf.data() + complete_length, json_buf.size() - complete_length, R"(" })");

    if (close_json < complete_length || close_json > json_buf.size() - 1) {
        ESP_LOGI("HttpLogSink", "Couldn't write rest of json");
        return false;
    }

    const auto successful = logSink.template writeData<HTTP_METHOD_POST>(json_buf.data(), close_json + 1);

    if (!successful) {
        ESP_LOGI("HttpLogSink", "Couldn't write to stream");
        return false;
    }

    return true;
}

/*
template<typename ... Arguments>
bool SdCardSink::log(LogLevel level, const char *fmt, Arguments && ... args) {
    return false;

    auto current_log_file = open_current_log();

    if (current_log_file == nullptr) {
        return false;
    }

    const auto written = fprintf(_log_output_file, fmt, std::forward<Arguments>(args) ...);

    potential_flush(written);

    return true; 

}
*/