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

        [[nodiscard]] bool isActive() const
        {
            return m_id != TaskId::invalid && PoolType::isValidTaskId(m_id);
        }

        void invalidate()
        {
            if (!isActive()) {
                return;
            }

            if (PoolType::removeTask(m_id))
            {
                m_id = TaskId::invalid;
            }
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
    bool is_being_executed = false;
};

// TODO: maybe use multiple threads
// TODO: use instance, instead of static functions
template<auto TaskPoolSize>
requires (ValidTaskPoolArgs<TaskPoolSize>)
class TaskPool {

    using TaskInfo = std::pair<TaskId, TaskDescription>;
    class TaskExecutionTracker
    {
        TaskInfo mTaskCopy;
    public:
        TaskExecutionTracker() : mTaskCopy(TaskId::invalid, {})
        {
        }

        explicit TaskExecutionTracker(const TaskInfo &info) : mTaskCopy(info)
        {
        }

        TaskExecutionTracker(TaskExecutionTracker&& other) noexcept : mTaskCopy(other.mTaskCopy)
        {
            other.mTaskCopy.first = TaskId::invalid;
        }

        TaskExecutionTracker &operator=(TaskExecutionTracker &&other) noexcept
        {
            using std::swap;
            swap(mTaskCopy, other.mTaskCopy);
            return *this;
        }

        ~TaskExecutionTracker()
        {
            finishExecution(mTaskCopy);
        }

        TaskId id() const { return mTaskCopy.first; }

        explicit operator bool() const { return mTaskCopy.first != TaskId::invalid; }

        TaskDescription &taskDescription() { return mTaskCopy.second; }

        TaskExecutionTracker(const TaskExecutionTracker &) = delete;
        TaskExecutionTracker &operator=(const TaskExecutionTracker &) = delete;
    };

    public:
        using TaskResourceType = TaskResourceTracker<TaskPool>;

        [[nodiscard]] static TaskResourceType postTask(TaskDescription task);
        [[nodiscard]] static bool removeTask(const TaskId& id);
        [[noreturn]] static void doWork();
        [[nodiscard]] static bool isValidTaskId(const TaskId& id);

    private:
        static auto handleTaskExecutions();

        static void finishExecution(TaskInfo info);
        static TaskExecutionTracker startExecution(TaskInfo info);
        [[nodiscard]] static bool updateTaskState(const TaskInfo& info);

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

    TaskExecutionTracker taskExecutor;
    {
        std::unique_lock instanceGuard{taskListMutex, std::defer_lock};
        if (!instanceGuard.try_lock_for(1000ms))
        {
            return nextRegularExecution;
        }

        auto toExecute = _tasks.end();

        for (auto currentTask = _tasks.begin(); currentTask != _tasks.end(); ++currentTask)
        {
            if (currentTask->first == TaskId::invalid)
            {
                continue;
            }

            if (currentTask->second.is_being_executed)
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

        taskExecutor = startExecution(*toExecute);
    }

    if (!taskExecutor)
    {
        return nextRegularExecution;
    }

    auto &currentTaskToExecute = taskExecutor.taskDescription();

    Logger::log(LogLevel::Info,
                            "=====================================[ In :%s ]======================================",
                            currentTaskToExecute.description);

    if (currentTaskToExecute.func_ptr) {
        currentTaskToExecute.func_ptr(currentTaskToExecute.argument);
    }

    Logger::log(LogLevel::Info,
                "=====================================[ Out : %s ] =====================================",
                currentTaskToExecute.description);

    if (currentTaskToExecute.single_shot) {
        const bool removedTask [[maybe_unused]] = removeTask(taskExecutor.id());
        return nextRegularExecution;
    }

    return std::min(nextRegularExecution, calculateNextExecutionTime(currentTaskToExecute));
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
auto TaskPool<TaskPoolSize>::startExecution(TaskInfo info) -> TaskExecutionTracker
{
    info.second.is_being_executed = true;
    if (updateTaskState(info))
    {
        return TaskExecutionTracker{info};
    }
    return TaskExecutionTracker{};
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
void TaskPool<TaskPoolSize>::finishExecution(TaskInfo info)
{
    info.second.is_being_executed = false;
    info.second.last_executed = std::chrono::steady_clock::now();
    const auto marked [[maybe_unused]] = updateTaskState(info);
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
bool TaskPool<TaskPoolSize>::updateTaskState(const TaskInfo& info)
{
    std::unique_lock instanceGuard{taskListMutex};

    bool found = false;
    (void)_tasks.modifyOrRemove([info, &found](auto& currentTaskPair)
    {
        if (currentTaskPair.first == info.first)
        {
            currentTaskPair.second = info.second;
            found = true;
        }
        return false;
    });

    return found;
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
                    && !currentTaskPair.second.is_being_executed
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
    TaskId createdId{};
    {
        std::unique_lock instance_guard{taskListMutex};

        task.is_being_executed = false;

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
bool TaskPool<TaskPoolSize>::isValidTaskId(const TaskId& id) {
    std::unique_lock instance_guard{taskListMutex};

    return std::any_of(_tasks.begin(), _tasks.end(), [id](const auto& currentTaskPair)
    {
        return currentTaskPair.first == id;
    });
}

template <auto TaskPoolSize> requires (ValidTaskPoolArgs<TaskPoolSize>)
bool TaskPool<TaskPoolSize>::removeTask(const TaskId& id) {
    if (id == TaskId::invalid) {
        return false;
    }

    {
        std::unique_lock instance_guard{taskListMutex};

        return _tasks.modifyOrRemove([id](const auto &currentTaskPair) {
            Logger::log(LogLevel::Info, "Removed task %s from pool", currentTaskPair.second.description);
            return currentTaskPair.first == id;
        });
    }
}