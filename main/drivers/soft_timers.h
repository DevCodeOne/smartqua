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

// TODO: improve and split into soft_timers.cpp
class soft_timer final {
    public:
        ~soft_timer();
        soft_timer(const soft_timer &) = delete;
        soft_timer(soft_timer && other) : m_timer(other.m_timer) { other.m_timer = nullptr; };

        soft_timer &operator=(const soft_timer &other) = delete;
        soft_timer &operator=(soft_timer &&other);

    private:
        soft_timer(single_timer_settings *timer);

        single_timer_settings *m_timer;

        friend std::optional<soft_timer> create_timer(single_timer_settings *);
        friend std::optional<soft_timer> create_timer(std::string_view, single_timer_settings &);
};

namespace Detail {
    template<size_t N>
    struct soft_timer_driver final {
        public:
            static bool add_timer(single_timer_settings *timer);
            static bool remove_timer(single_timer_settings *timer);

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

        auto copied_data = _data;

        bool is_already_in_use = std::find(copied_data.timers.cbegin(), copied_data.timers.cend(), timer) != copied_data.timers.cend();

        if (is_already_in_use) {
            ESP_LOGI("Soft_timer_driver", "Address already in use");
            return false;
        }

        unsigned int first_empty_space = 0;
        for (; first_empty_space < copied_data.timers.size(); ++first_empty_space) {
            if (copied_data.timers[first_empty_space] == nullptr) {
                copied_data.timers[first_empty_space] = timer;
                copied_data.timers_executed[first_empty_space] = false;
                break;
            }
        }

        if (first_empty_space == copied_data.timers.size()) {
            ESP_LOGI("Soft_timer_driver", "No space left");
            return false;
        }

        _data = copied_data;

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
                ESP_LOGI("Soft_timer_driver", "Removed address %p", timer);

                break;
            }
        }

        if (found_index == _data.timers.size()) {
            return false;
        }

        return true;
    }

    template<size_t N>
    void soft_timer_driver<N>::handle_timers(void *) {
        std::uint16_t last_day_resetted = 0;
        ESP_LOGI("Soft_timer_driver", "Entering handle_timers ...");
        sntp_clock clock{};
        std::tm timeinfo;
        std::time_t now;

        do {
            time(&now);
            localtime_r(&now, &timeinfo);
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } while (timeinfo.tm_year < 100);

        while (true) {
            auto global_loop_start = std::chrono::steady_clock::now();
            timer_data *local_data = nullptr;
            timer_data copied_data;

            time(&now);
            localtime_r(&now, &timeinfo);
            std::array<char, 64> timebuffer;

            {
                std::unique_lock instance_guard{_instance_mutex};
                copied_data = _data;
                // TODO: cleanup
                local_data = &copied_data;
            }

            // Reset execution flags at the start of the day
            if (local_data != nullptr && last_day_resetted != timeinfo.tm_yday) {
                ESP_LOGI("Soft_timer_driver", "Resetting timer execution flags ... ");
                std::unique_lock instance_guard{_instance_mutex};
                std::fill(local_data->timers_executed.begin(), local_data->timers_executed.end(), false);
                last_day_resetted = timeinfo.tm_yday;
            }

            strftime(timebuffer.data(), timebuffer.size(), "%c", &timeinfo);
            ESP_LOGI("Soft_timer_driver", "Searching for timers, which trigger at : %s", timebuffer.data());
            unsigned int current_index = 0;

            std::chrono::seconds current_time_in_seconds = std::chrono::seconds(timeinfo.tm_sec) 
                + std::chrono::minutes(timeinfo.tm_min) 
                + std::chrono::hours(timeinfo.tm_hour);
            single_timer_settings local_copy;
            for (; current_index < local_data->timers.size(); ++current_index) {
                auto loop_start = std::chrono::steady_clock::now();
                {
                    std::unique_lock instance_guard{_instance_mutex};
                    if (local_data->timers[current_index] == nullptr) {
                        ESP_LOGI("Soft_timer_driver", "Timer is null %d %p", current_index, local_data->timers[current_index]);
                        continue;
                    }

                    if (!local_data->timers[current_index]->enabled) {
                        ESP_LOGI("Soft_timer_driver", "Timer is not enabled");
                        continue;
                    }

                    // Already executed
                    if (local_data->timers_executed[current_index]) {
                        ESP_LOGI("Soft_timer_driver", "Already executed this timer");
                        continue;
                    }

                    // Is not active on this weekday
                    if (!(local_data->timers[current_index]->weekday_mask & 1 << timeinfo.tm_wday)) {
                        ESP_LOGI("Soft_timer_driver", "Not active on this day");
                        continue;
                    }

                    if (static_cast<uint32_t>(current_time_in_seconds.count()) < local_data->timers[current_index]->time_of_day) {
                        ESP_LOGI("Soft_timer_driver", "Not the time to trigger this timer %u %u", 
                            static_cast<uint32_t>(current_time_in_seconds.count()),
                            local_data->timers[current_index]->time_of_day);
                        continue;
                    }
                    local_copy = *local_data->timers[current_index];
                }

                ESP_LOGI("Soft_timer_driver", "Executing action");
                auto result = set_device_action(local_copy.device_index,
                    local_copy.payload.data(), std::strlen(local_copy.payload.data()));

                if (result.result == json_action_result_value::successfull) {
                    std::unique_lock instance_guard{_instance_mutex};
                    local_data->timers_executed[current_index] = true;
                    ESP_LOGI("Soft_timer_driver", "Executed action");
                } else {
                    ESP_LOGI("Soft_timer_driver", "Execution of timer wasn't successfull, retrying later");
                }

                std::this_thread::sleep_until(loop_start + std::chrono::seconds(2));
            }

            {
                std::unique_lock instance_guard{_instance_mutex};
                _data = copied_data;
                // TODO: cleanup
            }

            // The execution of this loop as a hole, should take at least five seconds
            std::this_thread::sleep_until(global_loop_start + std::chrono::seconds(8));
        }
    }
}

using soft_timer_driver = Detail::soft_timer_driver<max_num_timers>;

std::optional<soft_timer> create_timer(std::string_view input, single_timer_settings &timer_settings);

std::optional<soft_timer> create_timer(single_timer_settings *timer_settings);