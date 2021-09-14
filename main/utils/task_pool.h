#pragma once

#include <array>
#include <algorithm>
#include <cstdint> 
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <thread>
#include <chrono>

#include "utils/thread_utils.h"

using task_func_type = void(*)(void *);

enum struct task_id : uint64_t {
    invalid = std::numeric_limits<uint64_t>::max()
};

template<typename PoolType>
class task_resource_tracker {
    public:
        task_resource_tracker(void *resource = nullptr, task_id id = task_id::invalid) : m_resource(resource), m_id(id) { }

        task_resource_tracker(const task_resource_tracker &other) = delete;
        task_resource_tracker(task_resource_tracker &&other) : m_resource(other.m_resource), m_id(other.m_id) { 
            other.m_resource = nullptr;
            other.m_id = task_id::invalid; 
        }

        ~task_resource_tracker() { PoolType::remove_task(m_id); };

        task_resource_tracker &operator=(const task_resource_tracker &other) = delete;
        task_resource_tracker &operator=(task_resource_tracker &&other) {
            using std::swap;

            swap(m_resource, other.m_resource);
            swap(m_id, other.m_id);

            return *this;
        }

        void swap(task_resource_tracker &other) {
            using std::swap;

            swap(m_resource, other.m_resource);
            swap(m_id, other.m_id);
        }

        const void *resource() const { return m_resource; }

        task_id id() const { return m_id; }

    private:
        const void* m_resource = nullptr;
        task_id m_id;
};

struct single_task {
    bool single_shot = true;
    task_func_type func_ptr = nullptr;
    std::chrono::seconds interval = std::chrono::seconds{5};
    void *argument = nullptr;
    std::chrono::seconds last_executed = std::chrono::seconds{0};
};

// TODO: maybe use multiple threads
template<size_t TaskPoolSize>
class task_pool {
    public:
        using resource_type = task_resource_tracker<task_pool>;

        static resource_type post_task(single_task task);
        static bool remove_task(task_id id);
        static void do_work();
    private:
        static void task_pool_thread(void *ptr);

        static inline std::array<std::pair<task_id, std::optional<single_task>>, TaskPoolSize> _tasks;
        static inline std::shared_mutex _instance_mutex;
        static inline task_id _next_id = task_id(0);
};

template<size_t TaskPoolSize>
void task_pool<TaskPoolSize>::task_pool_thread(void *) {
    size_t index = 0;

    using last_executed_type = decltype(std::declval<single_task>().last_executed);

    while (1) {
        bool sleepThisRound = false;
        {
            ESP_LOGI(__PRETTY_FUNCTION__, "Loop");
            auto seconds_since_epoch = 
            std::chrono::duration_cast<last_executed_type>(std::chrono::steady_clock::now().time_since_epoch());

            std::unique_lock instance_guard{_instance_mutex};
            auto &[current_id, current_task] = _tasks[index];
            if (current_task) {

                if (std::chrono::abs(current_task->last_executed - seconds_since_epoch) > current_task->interval) {

                    ESP_LOGI("task_pool", "Executing this task now");

                    if (current_task->func_ptr != nullptr) {
                        current_task->func_ptr(current_task->argument);
                    }

                    if (current_task->single_shot) {
                        current_task = std::nullopt;
                    }

                    current_task->last_executed = seconds_since_epoch;
                }

            }
            index = (index + 1) % _tasks.size();
            sleepThisRound = index == 0 || !current_task.has_value();
        }

        if (sleepThisRound) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1s);
        }

    }
}

template<size_t TaskPoolSize>
void task_pool<TaskPoolSize>::do_work() {
    task_pool::task_pool_thread(nullptr);
}

// TODO: maybe use std::optional as return type
template<size_t TaskPoolSize>
auto task_pool<TaskPoolSize>::post_task(single_task task) -> resource_type {
    std::unique_lock instance_guard{_instance_mutex};

    auto result = std::find_if(_tasks.begin(), _tasks.end(), [](auto &current_task_pair) {
        return current_task_pair.second == std::nullopt;
    });

    if (result == _tasks.end()) {
        return resource_type(task.argument, task_id::invalid);
    }

    result->first = _next_id;
    result->second = task;

    using task_id_integral = std::underlying_type_t<decltype(_next_id)>;

    _next_id = static_cast<decltype(_next_id)>(task_id_integral(_next_id) + task_id_integral(1));

    return resource_type(task.argument, result->first);
}

template<size_t TaskPoolSize>
bool task_pool<TaskPoolSize>::remove_task(task_id id) {
    if (id == task_id::invalid) {
        return false;
    }

    {
        std::unique_lock instance_guard{_instance_mutex};

        auto result = std::find_if(_tasks.begin(), _tasks.end(), [id](auto &current_task_pair) {
            return current_task_pair.first == id;
        });

        if (result == _tasks.end()) {
            return false;
        }

        result->second = std::nullopt;
    }

    return true;
}