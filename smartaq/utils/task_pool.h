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

    private:
        static auto handleTaskExecutions();
        static void repostTask(const std::pair<TaskId, TaskDescription>& task);

        using TaskInfo = std::pair<TaskId, TaskDescription>;

        static inline FixedSizeOptionalArray<TaskInfo, TaskPoolSize> _tasks;
        static inline std::recursive_timed_mutex taskListMutex;
        static inline std::condition_variable_any notify;
        static inline TaskId _next_id = TaskId(0);
};

inline auto calculateNextExecutionTime(const TaskDescription &info) {
    return info.last_executed + info.interval;
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
auto TaskPool<TaskPoolSize>::handleTaskExecutions() {
    using namespace std::chrono;

    const auto nowSinceEpoch = steady_clock::now();
    // TODO: Add second thread to next regular execution?
    steady_clock::time_point nextRegularExecution{ nowSinceEpoch + milliseconds{2000 } };

    std::optional<TaskInfo> nextTask;
    {
        std::unique_lock instanceGuard{taskListMutex, std::defer_lock};
        if (!instanceGuard.try_lock_for(1000ms))
        {
            return nextRegularExecution;
        }

        decltype(_tasks.begin()) toExecute = _tasks.end();;

        for (auto currentTask = _tasks.begin(); currentTask != _tasks.end(); ++currentTask)
        {
            if (currentTask->first == TaskId::invalid)
            {
                continue;
            }

            // Logger::log(LogLevel::Debug, "Checking task %s", currentTask->second.description);

            if (toExecute == _tasks.end())
            {
                toExecute = currentTask;
                continue;
            }

            if (calculateNextExecutionTime(currentTask->second) < calculateNextExecutionTime(toExecute->second))
            {
                toExecute = currentTask;
            }
        }

        if (toExecute == _tasks.end())
        {
            return nextRegularExecution;
        }

        const auto thisWantsToExecuteAt = calculateNextExecutionTime(toExecute->second);
        if (thisWantsToExecuteAt > nowSinceEpoch)
        {
            return thisWantsToExecuteAt;
        }

        nextTask = *toExecute;
        (void) removeTask(toExecute->first);
    }

    if (!nextTask.has_value())
    {
        return nextRegularExecution;
    }

    auto &currentTaskToExecute = nextTask.value().second;

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

    currentTaskToExecute.last_executed = steady_clock::now();

    repostTask(*nextTask);

    return std::min(nextRegularExecution, calculateNextExecutionTime(currentTaskToExecute));
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
[[noreturn]] void TaskPool<TaskPoolSize>::doWork()
{
    using namespace std::chrono;
    while (true)
    {
        const auto nextExecutionAt = handleTaskExecutions();
        const auto now = steady_clock::now();
        if (now >= nextExecutionAt)
        {
            continue;
        }

        const auto remainingWait = std::min(duration_cast<milliseconds>(nextExecutionAt - now), 5000ms);

        std::unique_lock waitLock{taskListMutex};
        notify.wait_for(waitLock, remainingWait, []()
        {
            return !_tasks.empty() && std::any_of(_tasks.begin(), _tasks.end(), [](const auto& currentTaskPair)
            {
                const auto now = steady_clock::now();
                return currentTaskPair.first != TaskId::invalid
                    && calculateNextExecutionTime(currentTaskPair.second) < now;
            });
        });
    }
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
        std::unique_lock instance_guard{taskListMutex};

        const auto addedTask = _tasks.append(std::make_pair(_next_id, task));
        if (!addedTask) {
            return TaskResourceType(task.argument, TaskId::invalid);
        }

        // TODO: Maybe create method for that
        createdId = _next_id;
        _next_id = nextId(_next_id);

        Logger::log(LogLevel::Info, "Adding thread %s to pool", task.description);
    }

    notify.notify_one();

    return TaskResourceType{task.argument, createdId};
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
void TaskPool<TaskPoolSize>::repostTask(const TaskInfo& task) {
    {
        std::unique_lock instance_guard{taskListMutex};

        const auto addedTask = _tasks.append(task);
        if (!addedTask) {
            Logger::log(LogLevel::Error, "Failed to repost task %s", task.second.description);
            return;
        }

        Logger::log(LogLevel::Info, "Reposted task %s to pool", task.second.description);
    }

    notify.notify_one();
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
bool TaskPool<TaskPoolSize>::removeTask(TaskId& id) {
    if (id == TaskId::invalid) {
        return false;
    }

    {
        std::unique_lock instance_guard{taskListMutex};

        const auto found = _tasks.modifyOrRemove([id](const auto &currentTaskPair) {
            Logger::log(LogLevel::Info, "Removed task %s from pool", currentTaskPair.second.description);
            return currentTaskPair.first == id;
        });

        if (found)
        {
            id = TaskId::invalid;
        }

        return found;
    }
}