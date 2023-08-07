#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <cstdint>
#include <optional>
#include <chrono>
#include <utility>
#include <limits>

#include "build_config.h"
#include "utils/logger.h"

#include "esp_heap_caps_init.h"

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

template<typename PoolType>
class LargeBufferBorrower {
    public:

        LargeBufferBorrower(const LargeBufferBorrower &other) = delete;
        LargeBufferBorrower(LargeBufferBorrower &&other) : m_buffer_ptr(other.m_buffer_ptr), m_length(other.m_length) { 
            other.m_buffer_ptr = nullptr;
            other.m_length = 0;
        }

        ~LargeBufferBorrower() { PoolType::return_buffer(m_buffer_ptr); }

        LargeBufferBorrower &operator=(const LargeBufferBorrower &other) = delete;
        LargeBufferBorrower &operator=(LargeBufferBorrower &&other) {
            using std::swap;

            swap(m_buffer_ptr, other.m_buffer_ptr);
            swap(m_length, other.m_length);

            return *this;
        }

        char *data() { return m_buffer_ptr; }

        const char *data() const { return m_buffer_ptr; }

        size_t size() const { return m_length; }
    private:
        LargeBufferBorrower(char *buffer_ptr, size_t length) : m_buffer_ptr(buffer_ptr), m_length(length) { }

        char *m_buffer_ptr = nullptr;
        size_t m_length = 0;

        friend PoolType;
};

template<size_t BufferSize, BufferLocation BufferType>
class AllocatedBuffer;

template<size_t BufferSize>
class AllocatedBuffer<BufferSize, BufferLocation::heap> {
    public:
    using buffer_type = std::unique_ptr<char, CustomDelete>;

    // TODO: specify malloc method later on heap_caps_malloc vs malloc
    AllocatedBuffer() {
        #ifdef USE_PSRAM
        m_buffer.reset(reinterpret_cast<char *>(heap_caps_malloc(BufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
        #else
        m_buffer.reset(reinterpret_cast<char *>(malloc(BufferSize)));
        #endif
    }

    constexpr auto size() const { return BufferSize; }

    constexpr auto data() const { return m_buffer.get(); }

    constexpr auto data() { return m_buffer.get(); }

    bool isAvailable() const {
        return m_status == BufferStatus::available;
    }

    bool giveBack(char *ptr) {
        if (ptr != m_buffer.get()) {
            return false;
        }

        m_status = BufferStatus::available;
        return true;
    }

    template<typename CreationLambda>
    std::optional<std::invoke_result_t<CreationLambda, char *, size_t>> borrowBuffer(const CreationLambda &create) {
        if (!isAvailable()) {
            return std::nullopt;
        }

        m_status = BufferStatus::borrowed;
        return create(m_buffer.get(), BufferSize);
    }

private:
    std::unique_ptr<char, CustomDelete> m_buffer = nullptr;
    BufferStatus m_status = BufferStatus::available;
};

template<size_t BufferSize>
class AllocatedBuffer<BufferSize, BufferLocation::stack> {
    public:
    using buffer_type = std::array<char, BufferSize>;

    AllocatedBuffer() { }

    constexpr auto size() const { return BufferSize; }

    constexpr auto data() const { return m_buffer.data(); }

    constexpr auto data() { return m_buffer.data(); }

    bool isAvailable() const {
        return m_status == BufferStatus::available;
    }

    bool giveBack(char *ptr) {
        if (ptr != m_buffer.data()) {
            return false;
        }

        m_status = BufferStatus::available;
        return true;
    }

    template<typename CreationLambda>
    std::optional<std::invoke_result_t<CreationLambda, char *, size_t>> borrowBuffer(const CreationLambda &create) {
        if (!isAvailable()) {
            return std::nullopt;
        }

        m_status = BufferStatus::borrowed;
        return create(m_buffer.data(), BufferSize);
    }

    private:
    std::array<char, BufferSize> m_buffer{};
    BufferStatus m_status = BufferStatus::available;
};

// TODO: Add multiple sizes
template<size_t NumBuffers, size_t BufferSize, BufferLocation location = BufferLocation::heap>
class LargeBufferPool { 
    public:
        using AllocatedBufferType = AllocatedBuffer<BufferSize, location>;
        using LargeBufferBorrowerType = LargeBufferBorrower<LargeBufferPool<NumBuffers, BufferSize, location>>;
        
        LargeBufferPool();

        static std::optional<LargeBufferBorrowerType> get_free_buffer();
        // TODO: implement
        static std::optional<LargeBufferBorrowerType> wait_for_free_buffer(std::chrono::milliseconds ms);
    private:
        static bool return_buffer(char *ptr);
        static void initialize_buffers();

        static inline std::array<AllocatedBufferType, NumBuffers> _buffers{};
        static inline std::shared_mutex _instance_mutex{};

        friend LargeBufferBorrowerType;
};

template<size_t NumBuffers, size_t BufferSize, BufferLocation location>
auto LargeBufferPool<NumBuffers, BufferSize, location>::get_free_buffer() -> std::optional<LargeBufferBorrowerType> {
    // Leave this in, otherwise the destructor of large_buffer 
    // (move constructor because of the variable this will be moved in) will lead to a dead-lock
    std::unique_lock instance_guard{_instance_mutex};
    std::optional<LargeBufferBorrowerType> foundBuffer = std::nullopt;

    std::any_of(_buffers.begin(), _buffers.end(), [&foundBuffer](auto &current_buffer) {
        foundBuffer = current_buffer.borrowBuffer([](char *buffer, size_t size) {
            return LargeBufferBorrowerType(buffer, size);
        });
        return foundBuffer.has_value();
    });

    if (!foundBuffer.has_value()) {
        Logger::log(LogLevel::Warning, "Couldn't find free buffer");
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Gave away free buffer %p", foundBuffer->data());
    return foundBuffer;
}

template<size_t NumBuffers, size_t BufferSize, BufferLocation location>
bool LargeBufferPool<NumBuffers, BufferSize, location>::return_buffer(char *ptr) {
    if (ptr == nullptr) {
        return false;
    }

    std::unique_lock instance_guard{_instance_mutex};
    auto gaveBackBuffer = std::any_of(_buffers.begin(), _buffers.end(), [ptr](auto &current_buffer) {
        return current_buffer.giveBack(ptr);
    });

    if (!gaveBackBuffer) {
        Logger::log(LogLevel::Error, "Couldn't give back buffer %p", ptr);
        return false;
    }

    Logger::log(LogLevel::Info, "Got buffer %p back", ptr);

    return true;

}