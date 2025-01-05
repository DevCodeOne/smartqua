#pragma once

#include <array>
#include <mutex>
#include <cstdio>
#include <string_view>
#include <tuple>
#include <cstdarg>

#include "utils/filesystem_utils.h"
#include "utils/do_finally.h"

enum struct LogLevel {
    Debug, Info, Warning, Error
};

const char *to_string(LogLevel level);

// TODO: Create quite backend for unit-tests
class PrintfBackend {
    public:
        template<typename ... Arguments>
        static void log(LogLevel level, const char *fmt, Arguments &&... args) {
            printf(fmt, std::forward<Arguments>(args) ...);
            printf("\n");
        }

        static void install() { }
        static void uninstall() { }
};

template<typename Backend, typename ... Sinks>
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
        static inline LogLevel _ignoreLogsBelow;
        static inline DoFinally unistallHook{
            []() {
                std::apply([](auto &&... currentSink) {
                    (currentSink.uninstall(), ...);
                }, _sinks);

                Backend::uninstall();
            }
        };
};

template<typename Backend, typename ... Sinks>
void ApplicationLogger<Backend, Sinks ...>::initSinksAndInstall() {
    std::unique_lock sinkGuard{_sinkMutex};
    std::call_once(_initializedSinks, []() {
        std::apply([](auto && ...currentSink) {
            (currentSink.install(), ...);
        }, _sinks);

        Backend::install();
    });
}

template<typename Backend, typename ... Sinks>
void ApplicationLogger<Backend, Sinks ...>::install() {
    initSinksAndInstall();
}


template<typename Backend, typename ... Sinks>
void ApplicationLogger<Backend, Sinks ...>::ignoreLogsBelow(LogLevel level) {
    std::unique_lock sinkGuard{_sinkMutex};

    _ignoreLogsBelow = level;
}

template<typename Backend, typename ... Sinks>
template<typename ... Arguments>
bool ApplicationLogger<Backend, Sinks ...>::log(LogLevel level, const char *fmt, Arguments &&... args) {
    std::unique_lock sinkGuard{_sinkMutex};

    const auto loggedSuccessfully = std::apply([&](auto && ... currentSink) {
        std::array results{(currentSink.log(level, fmt, args ...), ...)};

        Backend::log(level, fmt, std::forward<Arguments>(args) ...);

        for (const auto currentResult : results) {
            if (!currentResult) {
                return false;
            }
        }

        return true;
    }, _sinks);

    return loggedSuccessfully;
}

class VoidSink final {
    public:
        bool install() { return true; }
        bool uninstall() { return true; }

        
        template<typename ... Arguments>
        bool log(LogLevel level, const char *fmt, Arguments && ... args) { return true; }
    private:
};
