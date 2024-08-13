#pragma once

#include <algorithm>
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <optional>
#include <shared_mutex>

#include "frozen.h"
#include "esp_vfs.h"

#include "stats_types.h"
#include "build_config.h"
#include "actions/device_actions.h"
#include "utils/idf_utils.h"
#include "utils/task_pool.h"
#include "utils/utils.h"
#include "utils/filesystem_utils.h"
#include "storage/sd_filesystem.h"

// TODO: replace with new update runtime stuff like devices class
template<typename DriverType>
class single_stat final {
    public:
        single_stat(const single_stat &other) = delete;
        single_stat(single_stat &&other);
        ~single_stat();

        single_stat &operator=(const single_stat &other) = delete;
        single_stat &operator=(single_stat &&other);

        static std::optional<single_stat> create(single_stat_settings *stat_settings);
    private:
        single_stat(single_stat_settings *stat_settings);

        single_stat_settings *m_stat_settings;

        template<size_t N>
        friend class stats_driver;
};

template<typename DriverType>
std::optional<single_stat<DriverType>> single_stat<DriverType>::create(single_stat_settings *stat_settings) {
    return DriverType::create_stat(stat_settings);
}

template<typename DriverType>
single_stat<DriverType>::single_stat(single_stat_settings *stat_settings) : m_stat_settings(stat_settings) { }

template<typename DriverType>
single_stat<DriverType>::single_stat(single_stat && other) : m_stat_settings(other.m_stat_settings) { other.m_stat_settings = nullptr; }

template<typename DriverType>
single_stat<DriverType>::~single_stat() { DriverType::remove_stat(m_stat_settings); }

template<typename DriverType>
single_stat<DriverType> &single_stat<DriverType>::operator=(single_stat &&other) {
    using std::swap;

    swap(m_stat_settings, other.m_stat_settings);
    return *this;
}

template<size_t N>
class stats_driver final {
    public:
        using stat_type = single_stat<stats_driver<N>>;

        static bool add_stat(single_stat_settings *settings);
        static bool remove_stat(single_stat_settings *settings);

        static std::optional<stat_type> create_stat(single_stat_settings *);
        static std::optional<stat_type> create_stat(std::string_view description, single_stat_settings &out);
    
    private:
        static void stats_driver_task(void *);
        static void init_task();

        struct stats_data {
            std::array<single_stat_settings *, N> stat_settings{};
        };

        static inline std::once_flag _task_initialized{};
        static inline std::shared_mutex _instance_mutex{};
        static inline stats_data _data;
};

template<size_t N>
bool stats_driver<N>::add_stat(single_stat_settings *settings) {
    init_task();

    std::unique_lock instance_guard{_instance_mutex};

    if (!settings) {
        return false;
    }

    bool is_already_in_use = std::find_if(_data.stat_settings.begin(), _data.stat_settings.end(), [settings](auto &current_setting) {
        if (current_setting == nullptr) {
            return false;
        }

        return current_setting == settings || current_setting->device_index == settings->device_index;
    }) != _data.stat_settings.end();

    if (is_already_in_use) {
        Logger::log(LogLevel::Warning, "Address already in use");
        return false; 
    }

    auto first_empty_space = std::find(_data.stat_settings.begin(), _data.stat_settings.end(), nullptr);

    if (first_empty_space == _data.stat_settings.end()) {
        Logger::log(LogLevel::Warning, "No space left");
        return false;
    }

    *first_empty_space = settings;

    return true;
}

template<size_t N>
bool stats_driver<N>::remove_stat(single_stat_settings *settings) {
    std::unique_lock instance_guard{_instance_mutex};

    if (!settings) {
        return false;
    }

    auto result = std::find(_data.stat_settings.begin(), _data.stat_settings.end(), settings);

    if (result == _data.stat_settings.end()) {
        return false;
    }

    Logger::log(LogLevel::Info, "Removed address %p", *result);
    *result = nullptr;
    return true;
}

template<size_t N>
auto stats_driver<N>::create_stat(std::string_view description, single_stat_settings &out_stat_settings) -> std::optional<stat_type> {
    unsigned int device_index = static_cast<unsigned int>(out_stat_settings.device_index);
    unsigned int stat_interval = static_cast<unsigned int>(out_stat_settings.stat_interval.count());

    json_scanf(description.data(), description.size(), R"({ device_index : %u, stat_interval_minutes : %u })", &device_index, &stat_interval);

    out_stat_settings.stat_interval = std::chrono::minutes(stat_interval);
    bool result = check_assign(out_stat_settings.device_index, device_index);

    if (!result) {
        return std::nullopt;
    }

    return stats_driver::create_stat(&out_stat_settings);
}

template<size_t N>
auto stats_driver<N>::create_stat(single_stat_settings *stat_settings) -> std::optional<stat_type> {
    if (!stats_driver::add_stat(stat_settings)) {
        Logger::log(LogLevel::Warning, "Stat is already created");
        return std::nullopt;
    }

    return std::make_optional(stat_type{stat_settings});
}

template<size_t N>
void stats_driver<N>::init_task() {
    std::call_once(_task_initialized, []{
        Logger::log(LogLevel::Info, "Adding stats_task to TaskPool");
        TaskPool<max_task_pool_size>::postTask(SingleTask{
                .single_shot = false,
                .func_ptr = stats_driver<N>::stats_driver_task,
                .interval = std::chrono::minutes{1},
                .argument = nullptr,
                .description = "Stats updater thread"
        });
    });
}

template<size_t N>
void stats_driver<N>::stats_driver_task(void *) {
    Logger::log(LogLevel::Info, "Checking on stats");
    std::unique_lock instance_guard{_instance_mutex};

    std::tm timeinfo;
    std::time_t now;

    wait_for_clock_sync(&now, &timeinfo);

    using namespace std::chrono;

    minutes minutes_since_midnight = minutes{timeinfo.tm_min} + hours{timeinfo.tm_hour};
    std::array<char, 256> data_out;
    std::array<char, 64> out_file;
    std::array<char, 36> out_path;

    const char *mount_point = "";
    snprintf(out_path.data(), out_path.size(), "%s/stats/%d", mount_point, timeinfo.tm_yday);
    auto output_folder_exists = ensure_path_exists(out_path.data());

    if (output_folder_exists) {
        Logger::log(LogLevel::Info, "Writing stats to folder : %s", out_path.data());
    } else {
        Logger::log(LogLevel::Error, "Folder : %s to write stats to couldn't be created", out_path.data());
    }

    for (auto &current_stat : _data.stat_settings) {
        if (!current_stat) {
            continue;
        }

        auto time_since_last_measure = std::chrono::abs(minutes_since_midnight - current_stat->last_checked);

        if (time_since_last_measure < current_stat->stat_interval) {

            Logger::log(LogLevel::Info, "Not yet taking stat of device %u, because %d < %d",
                current_stat->device_index,
                static_cast<int32_t>(time_since_last_measure.count()),
                static_cast<int32_t>(current_stat->stat_interval.count()));
            continue;
        }

        auto result = get_devices_action(current_stat->device_index, nullptr, 0, data_out.data(), data_out.size() - 1);

        if (result.result == json_action_result_value::successfull) {
            Logger::log(LogLevel::Info, "Got %s", data_out.data());
            data_out[result.answer_len] = '\0';
            current_stat->last_checked = minutes_since_midnight;

            snprintf(out_file.data(), out_file.size(), "%s/device_%d.csv", out_path.data(), current_stat->device_index);

            auto *output = std::fopen(out_file.data(), "a");
            DoFinally closeOp( [&output]() {
                std::fclose(output);
            });

            if (output == nullptr) {
                Logger::log(LogLevel::Error, "Couldn't open file: %s ", out_file.data());
                continue;
            }

            std::fprintf(output, "%lu; \"%s\"", static_cast<uint32_t>(minutes_since_midnight.count()), data_out.data());
            std::fputc('\n', output);

            Logger::log(LogLevel::Info, "Wrote to %s ", out_file.data());
        }
    }
}