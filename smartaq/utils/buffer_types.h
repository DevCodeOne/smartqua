#pragma once

#include <memory>

struct CustomDelete {
    void operator()(void *ptr) {
        free(ptr);
    }
};

enum struct BufferLocation {
    stack, heap
};

enum struct BufferStatus {
    borrowed, available
};
