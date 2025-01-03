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
#include <type_traits>

#include "utils/logger.h"

using TaskFuncType = void(*)(void *);

enum struct TaskId : uint64_t {
    invalid = std::numeric_limits<uint64_t>::max()
};

template<typename PoolType>
class TaskResourceTracker {
    public:
        explicit TaskResourceTracker(void *resource = nullptr, TaskId id = TaskId::invalid) : m_resource(resource), m_id(id) { }

        TaskResourceTracker(const TaskResourceTracker &other) = delete;
        TaskResourceTracker(TaskResourceTracker &&other) : m_resource(other.m_resource), m_id(other.m_id) {
            other.m_resource = nullptr;
            other.m_id = TaskId::invalid;
        }

        ~TaskResourceTracker() { PoolType::removeTask(m_id); };

        TaskResourceTracker &operator=(const TaskResourceTracker &other) = delete;
        TaskResourceTracker &operator=(TaskResourceTracker &&other) {
            using std::swap;

            swap(m_resource, other.m_resource);
            swap(m_id, other.m_id);

            return *this;
        }

        void swap(TaskResourceTracker &other) {
            using std::swap;

            swap(m_resource, other.m_resource);
            swap(m_id, other.m_id);
        }

        [[nodiscard]] const void *resource() const { return m_resource; }

        [[nodiscard]] TaskId id() const { return m_id; }

    private:
        const void* m_resource = nullptr;
        TaskId m_id;
};

struct TaskDescription {
    bool single_shot = true;
    TaskFuncType func_ptr = nullptr;
    std::chrono::seconds interval = std::chrono::seconds{5};
    void *argument = nullptr;
    const char *description = "No Description";
    std::chrono::seconds last_executed = std::chrono::seconds{0};
};

// TODO: maybe use multiple threads
template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
class TaskPool {
    public:
        using TaskResourceType = TaskResourceTracker<TaskPool>;

        static TaskResourceType postTask(TaskDescription task);
        static bool removeTask(TaskId id);
        static void doWork();
    private:
        [[noreturn]] static void task_pool_thread(void *ptr);

        static inline std::array<std::pair<TaskId, std::optional<TaskDescription>>, TaskPoolSize> _tasks;
        static inline std::shared_mutex _instance_mutex;
        static inline std::condition_variable_any notify;
        static inline TaskId _next_id = TaskId(0);
};

template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
void TaskPool<TaskPoolSize>::task_pool_thread(void *) {

    using TimeType = decltype(TaskDescription::last_executed);

    while (1) {
        unsigned int workedThreads = 0;
        for (size_t index = 0; index < TaskPoolSize; ++index) {
            auto seconds_since_epoch =
            std::chrono::duration_cast<TimeType>(std::chrono::steady_clock::now().time_since_epoch());

            std::unique_lock instance_guard{_instance_mutex};
            auto &[current_id, current_task] = _tasks[index];

            if (current_task && std::chrono::abs(current_task->last_executed - seconds_since_epoch) > current_task->interval) {

                Logger::log(LogLevel::Info, "=====================================[ In :%s ]======================================",
                            current_task->description);

                if (current_task->func_ptr != nullptr) {
                    current_task->func_ptr(current_task->argument);
                }

                Logger::log(LogLevel::Info, "=====================================[ Out : %s ] =====================================",
                            current_task->description);

                if (current_task->single_shot) {
                    current_task = std::nullopt;
                }

                current_task->last_executed = seconds_since_epoch;
                ++workedThreads;
            } else if (current_task) {
                Logger::log(LogLevel::Info, "It is not the time to trigger this task %s", current_task->description);
            }
        }

        Logger::log(LogLevel::Info, "Iterated all threads starting at the front");

        if (workedThreads == 0) {
            using namespace std::chrono_literals;
            std::unique_lock localLock{_instance_mutex};
            notify.wait_for(localLock, 10s);
        }

    }
}

template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
void TaskPool<TaskPoolSize>::doWork() {
    TaskPool::task_pool_thread(nullptr);
}

// TODO: maybe use std::optional as return type
template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
auto TaskPool<TaskPoolSize>::postTask(TaskDescription task) -> TaskResourceType {
    TaskId createdId;
    {
        std::unique_lock instance_guard{_instance_mutex};

        auto result = std::ranges::find_if(_tasks, [](auto &current_task_pair) {
            return current_task_pair.second == std::nullopt;
        });

        if (result == _tasks.end()) {
            return TaskResourceType(task.argument, TaskId::invalid);
        }

        using IdType = decltype(_next_id);

        result->first = _next_id;
        result->second = task;

        using TaskIdIntegral = std::underlying_type_t<IdType>;

        _next_id = static_cast<IdType>(TaskIdIntegral(_next_id) + 1);

        Logger::log(LogLevel::Info, "Adding thread %s to pool", task.description);

        createdId = result->first;
    }

    notify.notify_all();

    return TaskResourceType(task.argument, createdId);
}

template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
bool TaskPool<TaskPoolSize>::removeTask(TaskId id) {
    if (id == TaskId::invalid) {
        return false;
    }

    {
        std::unique_lock instance_guard{_instance_mutex};

        auto result = std::ranges::find_if(_tasks, [id](auto &current_task_pair) {
            return current_task_pair.first == id;
        });

        if (result == _tasks.end()) {
            return false;
        }

        result->second = std::nullopt;
    }

    return true;
}