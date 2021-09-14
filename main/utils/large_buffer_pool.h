#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <cstdint>
#include <optional>
#include <chrono>
#include <utility>
#include <limits>

#include "esp_log.h"

struct custom_delete {
    void operator()(void *ptr) { 
        free(ptr); 
    }
};

enum struct buffer_location {
    stack, heap
};

enum struct buffer_status {
    borrowed, available
};

template<typename PoolType>
class large_buffer {
    public:

        large_buffer(const large_buffer &other) = delete;
        large_buffer(large_buffer &&other) : m_buffer_ptr(other.m_buffer_ptr), m_length(other.m_length) { 
            other.m_buffer_ptr = nullptr;
            other.m_length = 0;
        }

        ~large_buffer() { PoolType::return_buffer(m_buffer_ptr); }

        large_buffer &operator=(const large_buffer &other) = delete;
        large_buffer &operator=(large_buffer &&other) {
            using std::swap;

            swap(m_buffer_ptr, other.m_buffer_ptr);
            swap(m_length, other.m_length);

            return *this;
        }

        char *data() { return m_buffer_ptr; }

        const char *data() const { return m_buffer_ptr; }

        size_t size() const { return m_length; }
    private:
        large_buffer(char *buffer_ptr, size_t length) : m_buffer_ptr(buffer_ptr), m_length(length) { }

        char *m_buffer_ptr = nullptr;
        size_t m_length = 0;

        friend PoolType;
};

template<size_t BufferSize, buffer_location BufferType>
class allocated_buffer;

template<size_t BufferSize>
class allocated_buffer<BufferSize, buffer_location::heap> {
    public:
    using buffer_type = std::unique_ptr<char, custom_delete>;

    // TODO: specify malloc method later on heap_caps_malloc vs malloc
    allocated_buffer() {
        // m_buffer.reset(reinterpret_cast<char *>(heap_caps_malloc(BufferSize, MALLOC_CAP_SPIRAM)));
        m_buffer.reset(reinterpret_cast<char *>(malloc(BufferSize)));
    }

    constexpr auto size() const { return BufferSize; }

    constexpr auto data() const { return m_buffer.get(); }

    constexpr auto data() { return m_buffer.get(); }

    bool isAvailable() const {
        return m_status == buffer_status::available;
    }

    bool giveBack(char *ptr) {
        if (ptr != m_buffer.get()) {
            return false;
        }

        m_status = buffer_status::available;
        return true;
    }

    template<typename CreationLambda>
    std::optional<std::invoke_result_t<CreationLambda, char *, size_t>> borrowBuffer(const CreationLambda &create) {
        if (!isAvailable()) {
            return std::nullopt;
        }

        m_status = buffer_status::borrowed;
        return create(m_buffer.get(), BufferSize);
    }

private:
    std::unique_ptr<char, custom_delete> m_buffer = nullptr;
    buffer_status m_status = buffer_status::available;
};

template<size_t BufferSize>
class allocated_buffer<BufferSize, buffer_location::stack> {
    public:
    using buffer_type = std::array<char, BufferSize>;

    allocated_buffer() { }

    constexpr auto size() const { return BufferSize; }

    constexpr auto data() const { return m_buffer.data(); }

    constexpr auto data() { return m_buffer.data(); }

    bool isAvailable() const {
        return m_status == buffer_status::available;
    }

    bool giveBack(char *ptr) {
        if (ptr != m_buffer.data()) {
            return false;
        }

        m_status = buffer_status::available;
        return true;
    }

    template<typename CreationLambda>
    std::optional<std::invoke_result_t<CreationLambda, char *, size_t>> borrowBuffer(const CreationLambda &create) {
        if (!isAvailable()) {
            return std::nullopt;
        }

        m_status = buffer_status::borrowed;
        return create(m_buffer.data(), BufferSize);
    }

    private:
    std::array<char, BufferSize> m_buffer{};
    buffer_status m_status = buffer_status::available;
};

// TODO: Add multiple sizes
template<size_t NumBuffers, size_t BufferSize, buffer_location location = buffer_location::heap>
class large_buffer_pool { 
    public:
        using buffer_type = allocated_buffer<BufferSize, location>;
        using large_buffer_type = large_buffer<large_buffer_pool<NumBuffers, BufferSize, location>>;
        
        large_buffer_pool();

        static std::optional<large_buffer_type> get_free_buffer();
        // TODO: implement
        static std::optional<large_buffer_type> wait_for_free_buffer(std::chrono::milliseconds ms);
    private:
        static bool return_buffer(char *ptr);
        static void initialize_buffers();

        static inline std::array<buffer_type, NumBuffers> _buffers{};
        static inline std::shared_mutex _instance_mutex{};

        friend large_buffer_type;
};

template<size_t NumBuffers, size_t BufferSize, buffer_location location>
auto large_buffer_pool<NumBuffers, BufferSize, location>::get_free_buffer() -> std::optional<large_buffer_type> {
    // Leave this in, otherwise the destructor of large_buffer 
    // (move constructor because of the variable this will be moved in) will lead to a dead-lock
    std::unique_lock instance_guard{_instance_mutex};
    std::optional<large_buffer_type> foundBuffer = std::nullopt;

    std::any_of(_buffers.begin(), _buffers.end(), [&foundBuffer](auto &current_buffer) {
        foundBuffer = current_buffer.borrowBuffer([](char *buffer, size_t size) {
            return large_buffer_type(buffer, size);
        });
        return foundBuffer.has_value();
    });

    if (!foundBuffer.has_value()) {
        ESP_LOGW("large_buffer_pool", "Couldn't find free buffer");
        return std::nullopt;
    }

    ESP_LOGI("large_buffer_pool", "Gave away free buffer %p", foundBuffer->data());
    return foundBuffer;
}

template<size_t NumBuffers, size_t BufferSize, buffer_location location>
bool large_buffer_pool<NumBuffers, BufferSize, location>::return_buffer(char *ptr) {
    if (ptr == nullptr) {
        return false;
    }

    std::unique_lock instance_guard{_instance_mutex};
    auto gaveBackBuffer = std::any_of(_buffers.begin(), _buffers.end(), [ptr](auto &current_buffer) {
        return current_buffer.giveBack(ptr);
    });

    if (!gaveBackBuffer) {
        ESP_LOGI("large_buffer_pool", "Couldn't give back buffer %p", ptr);
        return false;
    }

    ESP_LOGI("large_buffer_pool", "Got buffer %p back", ptr);

    return true;

}