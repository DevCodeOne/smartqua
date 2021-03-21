#pragma once

// For atomic
#include "esp_types.h"
#include "stdint.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <thread>
#include <string_view>
#include <shared_mutex>
#include <optional>
#include <mutex>
#include <type_traits>

#include "esp_log.h"

#include "smartqua_config.h"
#include "actions/device_actions.h"
#include "drivers/soft_timer_types.h"
#include "utils/utils.h"
#include "utils/idf-utils.h"
#include "utils/thread_utils.h"

template<typename DriverType>
class soft_timer final {
    public:
        soft_timer(const soft_timer &) = delete;
        soft_timer(soft_timer && other);
        ~soft_timer();

        soft_timer &operator=(const soft_timer &other) = delete;
        soft_timer &operator=(soft_timer &&other);

    private:
        soft_timer(single_timer_settings *timer);

        single_timer_settings *m_timer;

        template<size_t N>
        friend class soft_timer_driver;
};

template<typename DriverType>
soft_timer<DriverType>::soft_timer(single_timer_settings *timer) : m_timer(timer) { }

template<typename DriverType>
soft_timer<DriverType>::soft_timer(soft_timer && other) : m_timer(other.m_timer) { other.m_timer = nullptr; };

template<typename DriverType>
soft_timer<DriverType>::~soft_timer() { 
    DriverType::remove_timer(m_timer);
}

template<typename DriverType>
soft_timer<DriverType> &soft_timer<DriverType>::operator=(soft_timer &&other) { 
    using std::swap;
    std::swap(m_timer, other.m_timer);
    return *this;
};

template<size_t N>
struct soft_timer_driver final {
    public:
        using timer_type = soft_timer<soft_timer_driver<N>>;

        static bool add_timer(single_timer_settings *timer);
        static bool remove_timer(single_timer_settings *timer);

        static std::optional<timer_type> create_timer(std::string_view input, single_timer_settings &timer_settings);
        static std::optional<timer_type> create_timer(single_timer_settings *timer_settings);

    private:
        static void handle_timers(void *);

        struct timer_data {
            std::array<single_timer_settings *, N> timers{};
            std::array<bool, N> timers_executed{};
        };

        static inline std::once_flag _thread_initialized{};
        static inline std::shared_mutex _instance_mutex{};
        static inline timer_data _data;
};

template<size_t N>
bool soft_timer_driver<N>::add_timer(single_timer_settings *timer) {
    std::unique_lock instance_guard{_instance_mutex};

    if (timer == nullptr) {
        return false;
    }

    std::call_once(_thread_initialized, 
        [](){ 
            thread_creator::create_thread(soft_timer_driver::handle_timers, "TimerTask", nullptr); 
        });

    bool is_already_in_use = std::find(_data.timers.cbegin(), _data.timers.cend(), timer) != _data.timers.cend();

    if (is_already_in_use) {
        ESP_LOGI("Soft_timer_driver", "Address already in use");
        return false;
    }

    unsigned int first_empty_space = 0;
    for (; first_empty_space < _data.timers.size(); ++first_empty_space) {
        if (_data.timers[first_empty_space] == nullptr) {
            _data.timers[first_empty_space] = timer;
            _data.timers_executed[first_empty_space] = false;
            break;
        }
    }

    if (first_empty_space == _data.timers.size()) {
        ESP_LOGI("Soft_timer_driver", "No space left");
        return false;
    }

    return true;
}

template<size_t N>
bool soft_timer_driver<N>::remove_timer(single_timer_settings *timer) {
    std::unique_lock instance_guard{_instance_mutex};

    if (timer == nullptr) {
        return false;
    }
    
    unsigned int found_index = 0;
    for (; found_index < _data.timers.size(); ++found_index) {
        if (_data.timers[found_index] == timer) {
            _data.timers[found_index] = nullptr;
            _data.timers_executed[found_index] = false;
            ESP_LOGI("Soft_timer_driver", "Removed address %p", timer);

            break;
        }
    }

    return found_index != _data.timers.size();
}

template<size_t N>
void soft_timer_driver<N>::handle_timers(void *) {
    std::uint16_t last_day_resetted = 0;
    ESP_LOGI("Soft_timer_driver", "Entering handle_timers ...");

    std::time_t now;
    std::tm timeinfo;

    wait_for_clock_sync(&now, &timeinfo);

    while (true) {
        auto global_loop_start = std::chrono::steady_clock::now();

        time(&now);
        localtime_r(&now, &timeinfo);
        std::array<char, 64> timebuffer;

        // Reset execution flags at the start of the day
        if (last_day_resetted != timeinfo.tm_yday) {
            ESP_LOGI("Soft_timer_driver", "Resetting timer execution flags ... ");
            std::unique_lock instance_guard{_instance_mutex};
            std::fill(_data.timers_executed.begin(), _data.timers_executed.end(), false);
            last_day_resetted = timeinfo.tm_yday;
        }

        strftime(timebuffer.data(), timebuffer.size(), "%c", &timeinfo);
        ESP_LOGI("Soft_timer_driver", "Searching for timers, which trigger at : %s", timebuffer.data());
        unsigned int current_index = 0;

        using namespace std::chrono;
        seconds current_time_in_seconds = seconds(timeinfo.tm_sec) + minutes(timeinfo.tm_min) + hours(timeinfo.tm_hour);

        single_timer_settings local_copy;
        for (; current_index < _data.timers.size(); ++current_index) {
            auto loop_start = std::chrono::steady_clock::now();
            {
                std::unique_lock instance_guard{_instance_mutex};
                if (_data.timers[current_index] == nullptr) {
                    ESP_LOGI("Soft_timer_driver", "Timer is null %d %p", current_index, _data.timers[current_index]);
                    continue;
                }

                if (!_data.timers[current_index]->enabled) {
                    ESP_LOGI("Soft_timer_driver", "Timer is not enabled");
                    continue;
                }

                // Already executed
                if (_data.timers_executed[current_index]) {
                    ESP_LOGI("Soft_timer_driver", "Already executed this timer");
                    continue;
                }

                // Is not active on this weekday
                if (!(_data.timers[current_index]->weekday_mask & 1 << timeinfo.tm_wday)) {
                    ESP_LOGI("Soft_timer_driver", "Not active on this day");
                    continue;
                }

                if (static_cast<uint32_t>(current_time_in_seconds.count()) < _data.timers[current_index]->time_of_day) {
                    ESP_LOGI("Soft_timer_driver", "Not the time to trigger this timer %u %u", 
                        static_cast<uint32_t>(current_time_in_seconds.count()),
                        _data.timers[current_index]->time_of_day);
                    continue;
                }
                local_copy = *_data.timers[current_index];
            }

            ESP_LOGI("Soft_timer_driver", "Executing action");
            auto result = set_device_action(local_copy.device_index,
                local_copy.payload.data(), std::strlen(local_copy.payload.data()), nullptr, 0);

            if (result.result == json_action_result_value::successfull) {
                std::unique_lock instance_guard{_instance_mutex};
                _data.timers_executed[current_index] = true;
                ESP_LOGI("Soft_timer_driver", "Executed action");
            } else {
                ESP_LOGI("Soft_timer_driver", "Execution of timer wasn't successfull, retrying later");
            }

            std::this_thread::sleep_until(loop_start + std::chrono::seconds(2));
        }

        // The execution of this loop as a hole, should take at least five seconds
        std::this_thread::sleep_until(global_loop_start + std::chrono::seconds(8));
    }
}

template<size_t N>
auto soft_timer_driver<N>::create_timer(std::string_view input, single_timer_settings &timer_settings) -> std::optional<timer_type> {
    json_token token{};
    uint32_t weekday_mask = timer_settings.weekday_mask;
    // %hhu doesn't work
    json_scanf(input.data(), input.size(), "{ time_of_day : %u, enabled : %B, weekday_mask : %u, device_index : %u, payload : %T }", 
        &timer_settings.time_of_day, 
        &timer_settings.enabled,
        &weekday_mask,
        &timer_settings.device_index,
        &token);

    timer_settings.weekday_mask = static_cast<uint8_t>(weekday_mask);
    
    if (token.len + 1 > static_cast<int>(timer_settings.payload.size())) {
        ESP_LOGI("Soft_timer_driver", "Payload was too large %d : %d", token.len, timer_settings.payload.size());
        return std::nullopt;
    }

    std::strncpy(timer_settings.payload.data(), token.ptr, token.len);
    *(timer_settings.payload.data() + token.len) = '\0';

    return create_timer(&timer_settings);
}

template<size_t N>
auto soft_timer_driver<N>::create_timer(single_timer_settings *timer_settings) -> std::optional<timer_type> {
    if (!soft_timer_driver::add_timer(timer_settings)) {
        return std::nullopt;
    }

    return std::make_optional(timer_type{timer_settings});
}