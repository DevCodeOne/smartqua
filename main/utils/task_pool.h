#pragma once

#include <array>
#include <cstdint> 
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <thread>

#include "utils/thread_utils.h"

using task_func_type = void(*)(void *);

struct single_task {
    bool single_shot = true;
    task_func_type func_ptr = nullptr;
    std::chrono::seconds interval = std::chrono::seconds{5};
    void *argument = nullptr;
};

enum struct task_id : uint64_t {
    invalid = std::numeric_limits<uint64_t>::max()
};

// TODO: maybe use multiple threads
template<size_t TaskPoolSize>
class task_pool {
    public:
        static task_id post_task(single_task task);
        static bool remove_task(task_id id);
    private:
        static void task_pool_thread(void *ptr);
        static void init_thread();

        static inline std::array<std::pair<task_id, std::optional<single_task>>, TaskPoolSize> _tasks;
        static inline std::shared_mutex _instance_mutex;
        static inline task_id _next_id = task_id(0);
        static inline std::once_flag _thread_init;
};

template<size_t TaskPoolSize>
void task_pool<TaskPoolSize>::task_pool_thread(void *ptr) {
    size_t index = 0;
    while (1) {
        {
            std::unique_lock instance_guard{_instance_mutex};
            auto &[current_id, current_task] = _tasks[index];
            if (current_task) {
                // TODO: maybe add something like a timestamp, so a task can be executed only after a certain amount of time
                if (current_task->func_ptr != nullptr) {
                    current_task->func_ptr(current_task->argument);
                }

                if (current_task->single_shot) {
                    current_task = std::nullopt;
                }
            }
            index = (index + 1) % _tasks.size();
            if (current_task || index == 0) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }

    }
}

template<size_t TaskPoolSize>
void task_pool<TaskPoolSize>::init_thread() {
    std::call_once(_thread_init, []() {
        thread_creator::create_thread(task_pool::task_pool_thread, "task_pool_thread");
    });
}

template<size_t TaskPoolSize>
task_id task_pool<TaskPoolSize>::post_task(single_task task) {
    init_thread();

    std::unique_lock instance_guard{_instance_mutex};

    auto result = std::find_if(_tasks.begin(), _tasks.end(), [](auto &current_task_pair) {
        return current_task_pair.second == std::nullopt;
    });

    if (result == _tasks.end()) {
        return task_id::invalid;
    }

    result->first = _next_id;
    result->second = task;

    using task_id_integral = std::underlying_type_t<decltype(_next_id)>;

    _next_id = static_cast<decltype(_next_id)>(task_id_integral(_next_id) + task_id_integral(1));

    return result->first;
}

template<size_t TaskPoolSize>
bool task_pool<TaskPoolSize>::remove_task(task_id id) {
    init_thread();

    std::unique_lock instance_guard{_instance_mutex};

    auto result = std::find_if(_tasks.begin(), _tasks.end(), [id](auto &current_task_pair) {
        return current_task_pair.first == id;
    });

    if (result == _tasks.end()) {
        return false;
    }

    result->second = std::nullopt;

    return true;
}