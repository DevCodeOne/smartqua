#pragma once

enum struct ThreadSafety {
    Safe, Unsafe
};

template<ThreadSafety Safety>
class ConditionalThreadSafety;

struct DoNothing {};

template<>
class ConditionalThreadSafety<ThreadSafety::Unsafe> {
public:
    auto createSharedLock() { return DoNothing{}; }
    auto createUniqueLock() { return DoNothing{}; }
};

template<>
class ConditionalThreadSafety<ThreadSafety::Safe> {
public:
    auto createSharedLock() { return std::shared_lock{mutex}; }
    auto createUniqueLock() { return std::unique_lock{mutex}; }

    std::shared_mutex mutex;
};