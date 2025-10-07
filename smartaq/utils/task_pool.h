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
#include <condition_variable>
#include <type_traits>

#include "utils/logger.h"
#include "utils/container/fixed_size_optional_array.h"

using TaskFuncType = void(*)(void *);

enum struct TaskId : uint64_t {
    invalid = std::numeric_limits<uint64_t>::max()
};

template<auto PoolSize>
concept ValidTaskPoolArgs = (std::is_unsigned_v<decltype(PoolSize)>);

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
            invalidate();
        };

        TaskResourceTracker &operator=(const TaskResourceTracker &other) = delete;
        TaskResourceTracker &operator=(TaskResourceTracker &&other) noexcept {
            using std::swap;

            swap(m_resource, other.m_resource);
            swap(m_id, other.m_id);

            return *this;
        }

        bool isActive()
        {
            return m_id != TaskId::invalid;
        }

        void invalidate()
        {
            if (!isActive()) {
                return;
            }

            const auto removed [[maybe_unused]] = PoolType::removeTask(m_id);
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
requires (ValidTaskPoolArgs<TaskPoolSize>)
class TaskPool {
    public:
        using TaskResourceType = TaskResourceTracker<TaskPool>;

        [[nodiscard]] static TaskResourceType postTask(TaskDescription task);
        [[nodiscard]] static bool removeTask(TaskId& id);
        [[noreturn]] static void doWork();
        static auto doWorkOnce();
    private:
        static auto handleTaskExecutions();
        static void repostTask(const std::pair<TaskId, TaskDescription>& task);

        [[noreturn]] static void task_pool_thread(void *ptr);

        using TaskInfo = std::pair<TaskId, TaskDescription>;

        static inline FixedSizeOptionalArray<TaskInfo, TaskPoolSize> _tasks;
        static inline std::shared_timed_mutex instanceMutex;
        static inline std::condition_variable_any notify;
        static inline TaskId _next_id = TaskId(0);
};

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
auto TaskPool<TaskPoolSize>::handleTaskExecutions() {
    using namespace std::chrono;

    const auto nowSinceEpoch = steady_clock::now();
    steady_clock::time_point nextRegularExecution{ nowSinceEpoch + milliseconds{10 } };

    std::optional<TaskInfo> toExecute;
    {
        if (!instanceMutex.try_lock_for(1000ms))
        {
            return nextRegularExecution;
        }

        std::unique_lock instance_guard{instanceMutex, std::adopt_lock};

        const auto foundTaskToExecute [[maybe_unused]] = _tasks.modifyOrRemove(
            [&nextRegularExecution, &nowSinceEpoch, &toExecute](
            TaskInfo& currentTask)
            {
                if (toExecute)
                {
                    return false;
                }

                auto& currentTaskInfo = currentTask.second;
                const auto currentTaskNextExecutionAt = currentTaskInfo.last_executed +
                    currentTaskInfo.interval;

                nextRegularExecution = std::min(
                    currentTaskNextExecutionAt, nextRegularExecution);

                if (currentTaskNextExecutionAt <= nowSinceEpoch)
                {
                    toExecute = currentTask;
                    return true;
                }

                return false;
            });
    }

    if (!toExecute)
    {
        return nextRegularExecution;
    }

    Logger::log(LogLevel::Info, "Found task to execute %s", toExecute->second.description);

    auto &currentTaskToExecute = toExecute.value().second;

    Logger::log(LogLevel::Info,
                            "=====================================[ In :%s ]======================================",
                            currentTaskToExecute.description);


    if (currentTaskToExecute.func_ptr != nullptr) {
        currentTaskToExecute.func_ptr(currentTaskToExecute.argument);
    }

    Logger::log(LogLevel::Info,
                "=====================================[ Out : %s ] =====================================",
                currentTaskToExecute.description);

    if (currentTaskToExecute.single_shot) {
        return nextRegularExecution;
    }

    currentTaskToExecute.last_executed = nowSinceEpoch;

    repostTask(*toExecute);

    return nextRegularExecution;
}

// TODO: Add method which executes once, instead of iterating endlessly
template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
void TaskPool<TaskPoolSize>::task_pool_thread(void *) {
    while (1) {
        const auto nextExecutionAt = handleTaskExecutions();
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextExecutionAt) {
            continue;
        }

        Logger::log(LogLevel::Info, "Iterated all threads starting at the front");

        using namespace std::chrono_literals;

        if (instanceMutex.try_lock_for(std::min(nextExecutionAt, now + 1000ms)))
        {
            std::unique_lock localLock{instanceMutex, std::adopt_lock};
            notify.wait_for(localLock, nextExecutionAt - now);
        }

    }
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
[[noreturn]] void TaskPool<TaskPoolSize>::doWork() {
    while (true) {
        auto nextExecutionAt = doWorkOnce();
        std::this_thread::sleep_until(nextExecutionAt);
    }
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
auto TaskPool<TaskPoolSize>::doWorkOnce() {
    return handleTaskExecutions();
}

template<typename IdType>
requires (std::is_enum_v<IdType>)
constexpr auto nextId(IdType currentId) {
    return static_cast<IdType>(static_cast<std::underlying_type_t<IdType>>(currentId) + 1);
}

// TODO: maybe use std::optional as return type
template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
auto TaskPool<TaskPoolSize>::postTask(TaskDescription task) -> TaskResourceType {
    TaskId createdId;
    {
        std::unique_lock instance_guard{instanceMutex};

        const auto addedTask = _tasks.append(std::make_pair(_next_id, task));
        if (!addedTask) {
            return TaskResourceType(task.argument, TaskId::invalid);
        }

        createdId = _next_id;

        _next_id = nextId(_next_id);

        Logger::log(LogLevel::Info, "Adding thread %s to pool", task.description);
    }

    notify.notify_all();

    return TaskResourceType{task.argument, createdId};
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
void TaskPool<TaskPoolSize>::repostTask(const TaskInfo& task) {
    {
        std::unique_lock instance_guard{instanceMutex};

        const auto addedTask = _tasks.append(task);
        if (!addedTask) {
            return;
        }

        Logger::log(LogLevel::Info, "Adding thread %s to pool", task.second.description);
    }

    notify.notify_all();
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
bool TaskPool<TaskPoolSize>::removeTask(TaskId& id) {
    if (id == TaskId::invalid) {
        return false;
    }

    {
        std::unique_lock instance_guard{instanceMutex};

        const auto found = _tasks.modifyOrRemove([id](auto &current_task_pair) {
            return current_task_pair.first == id;
        });

        if (found)
        {
            id = TaskId::invalid;
        }

        return found;
    }
}