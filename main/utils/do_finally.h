#pragma once

#include <type_traits>

template<typename Callable>
requires (std::is_invocable_v<Callable>)
class DoFinally final {
public:
    explicit DoFinally(Callable callable) : mCallable(std::move(callable)) {}
    ~DoFinally() { mCallable(); }

private:
    Callable mCallable;
};
