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
#include "utils/container/fixed_size_optional_array.h"

using TaskFuncType = void(*)(void *);

enum struct TaskId : uint64_t {
    invalid = std::numeric_limits<uint64_t>::max()
};

template<typename PoolType>
class TaskResourceTracker {
public:
        explicit TaskResourceTracker(void *resource = nullptr, TaskId id = TaskId::invalid) : m_resource(resource), m_id(id) { }

        TaskResourceTracker(const TaskResourceTracker &other) = delete;
        TaskResourceTracker(TaskResourceTracker &&other) noexcept : m_resource(other.m_resource), m_id(other.m_id) {
            other.m_resource = nullptr;
            other.m_id = TaskId::invalid;
        }

        ~TaskResourceTracker() {
            if (m_id == TaskId::invalid) {
                return;
            }

            PoolType::removeTask(m_id);
        };

        TaskResourceTracker &operator=(const TaskResourceTracker &other) = delete;
        TaskResourceTracker &operator=(TaskResourceTracker &&other) noexcept {
            using std::swap;

            swap(m_resource, other.m_resource);
            swap(m_id, other.m_id);

            return *this;
        }

        void swap(TaskResourceTracker &other) noexcept {
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
    std::chrono::milliseconds interval = std::chrono::milliseconds{5};
    void *argument = nullptr;
    const char *description = "No Description";
    std::chrono::steady_clock::time_point last_executed;
};

// TODO: maybe use multiple threads
// TODO: use instance, instead of static functions
template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
class TaskPool {
    public:
        using TaskResourceType = TaskResourceTracker<TaskPool>;

        static TaskResourceType postTask(TaskDescription task);
        static bool removeTask(TaskId id);
        static void doWork();
        static auto doWorkOnce();
    private:
        static auto handleTaskExecutions();

        [[noreturn]] static void task_pool_thread(void *ptr);

        using TaskInfo = std::pair<TaskId, TaskDescription>;

        static inline FixedSizeOptionalArray<TaskInfo, TaskPoolSize> _tasks;
        static inline std::shared_mutex _instance_mutex;
        static inline std::condition_variable_any notify;
        static inline TaskId _next_id = TaskId(0);
};

template <auto TaskPoolSize> requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
auto TaskPool<TaskPoolSize>::handleTaskExecutions() {
    std::unique_lock instance_guard{_instance_mutex};
    using namespace std::chrono;

    const auto nowSinceEpoch = steady_clock::now();
    steady_clock::time_point nextRegularExecution{ nowSinceEpoch + milliseconds{10 } };

    [[maybe_unused]] const auto hasFinishedAnyTasks = _tasks.modifyOrRemove([&nextRegularExecution, &nowSinceEpoch](TaskInfo &currentTask) {
        auto &current_task = currentTask.second;
        const auto currentTaskNextExecutionAt = current_task.last_executed + current_task.interval;

        nextRegularExecution = std::min(currentTaskNextExecutionAt, nextRegularExecution);

        if (currentTaskNextExecutionAt <= nowSinceEpoch) {
            Logger::log(LogLevel::Info,
                        "=====================================[ In :%s ]======================================",
                        current_task.description);

            if (current_task.func_ptr != nullptr) {
                current_task.func_ptr(current_task.argument);
            }

            Logger::log(LogLevel::Info,
                        "=====================================[ Out : %s ] =====================================",
                        current_task.description);

            if (current_task.single_shot) {
                return true;
            }

            current_task.last_executed = nowSinceEpoch;
        } else {
            Logger::log(LogLevel::Info, "It is not the time to trigger this task %s",
                        current_task.description);
        }

        return false;
    });

    return nextRegularExecution;
}

// TODO: Add method which executes once, instead of iterating endlessly
template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
void TaskPool<TaskPoolSize>::task_pool_thread(void *) {
    while (1) {
        const auto nextExecutionAt = handleTaskExecutions();
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        if (now >= nextExecutionAt) {
            continue;
        }

        Logger::log(LogLevel::Info, "Iterated all threads starting at the front");

        using namespace std::chrono_literals;
        std::unique_lock localLock{_instance_mutex};
        notify.wait_for(localLock, nextExecutionAt - now);
    }
}

template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
void TaskPool<TaskPoolSize>::doWork() {
    while (1) {
        auto nextExecutionAt = doWorkOnce();
        std::this_thread::sleep_until(nextExecutionAt);
    }
}

template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
auto TaskPool<TaskPoolSize>::doWorkOnce() {
    return handleTaskExecutions();
}

template<typename IdType>
requires (std::is_enum_v<IdType>)
constexpr auto nextId(IdType currentId) {
    return static_cast<IdType>(static_cast<std::underlying_type_t<IdType>>(currentId) + 1);
}

// TODO: maybe use std::optional as return type
template<auto TaskPoolSize>
requires (std::is_unsigned_v<decltype(TaskPoolSize)>)
auto TaskPool<TaskPoolSize>::postTask(TaskDescription task) -> TaskResourceType {
    TaskId createdId;
    {
        std::unique_lock instance_guard{_instance_mutex};

        const auto addedTask = _tasks.append(std::make_pair(_next_id, task));
        if (!addedTask) {
            return TaskResourceType(task.argument, TaskId::invalid);
        }

        createdId = _next_id;

        _next_id = nextId(_next_id);

        Logger::log(LogLevel::Info, "Adding thread %s to pool", task.description);
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

        return _tasks.modifyOrRemove([id](auto &current_task_pair) {
            return current_task_pair.first == id;
        });
    }
}