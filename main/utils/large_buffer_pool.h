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

template<typename PoolType>
class large_buffer {
    public:

        large_buffer(const large_buffer &other) = delete;
        large_buffer(large_buffer &&other) : m_buffer(std::move(other.m_buffer)) { }
        ~large_buffer() { PoolType::return_buffer(std::move(m_buffer)); }

        large_buffer &operator=(const large_buffer &other) = delete;
        large_buffer &operator=(large_buffer &&other) {
            using std::swap;

            swap(m_buffer, other.m_buffer);

            return *this;
        }

        char *data() { return m_buffer.get(); }

        const char *data() const { return m_buffer.get(); }

        size_t size() const { return PoolType::buffer_size; }
    private:
        large_buffer(typename PoolType::buffer_type &&buffer) : m_buffer(std::move(buffer)) { }

        typename PoolType::buffer_type m_buffer = nullptr;

        friend PoolType;
};

// TODO: Add multiple sizes
template<size_t NumBuffers, size_t BufferSize>
class large_buffer_pool { 
    public:
        static inline constexpr auto buffer_size = BufferSize;

        using large_buffer_type = large_buffer<large_buffer_pool<NumBuffers, BufferSize>>;
        using buffer_type = std::unique_ptr<char, custom_delete>;

        static std::optional<large_buffer_type> get_free_buffer();
        // TODO: implement
        static std::optional<large_buffer_type> wait_for_free_buffer(std::chrono::milliseconds ms);
    private:
        static bool return_buffer(buffer_type buffer);
        static void initialize_buffers();

        static inline std::array<buffer_type, NumBuffers> _buffers{};
        static inline std::shared_mutex _instance_mutex{};
        static inline std::once_flag _init_buffers_flag{};

        friend large_buffer_type;
};

template<size_t NumBuffers, size_t BufferSize>
auto large_buffer_pool<NumBuffers, BufferSize>::get_free_buffer() -> std::optional<large_buffer_type> {
    buffer_type found_buffer = nullptr;
    // Leave this in, otherwise the destructor of large_buffer 
    // (move constructor because of the variable this will be moved in) will lead to a dead-lock
    {
        std::unique_lock instance_guard{_instance_mutex};

        std::call_once(_init_buffers_flag, []() {
            for (auto &current_buffer : _buffers) {
                current_buffer.reset(reinterpret_cast<char *>(malloc(buffer_size)));
            }
        });

        auto result = std::find_if(_buffers.begin(), _buffers.end(), [](auto &current_buffer) {
            return current_buffer != nullptr;
        });

        if (result != _buffers.end()) {
            found_buffer = std::move(*result);
        }
        
    }
    
    if (found_buffer == nullptr) {
        return std::nullopt;
    }

    ESP_LOGI("large_buffer_pool", "Gave away free buffer %p", found_buffer.get());

    return large_buffer_type(std::move(found_buffer));
}

template<size_t NumBuffers, size_t BufferSize>
bool large_buffer_pool<NumBuffers, BufferSize>::return_buffer(buffer_type buffer) {
    if (buffer == nullptr) {
        return false;
    }

    std::unique_lock instance_guard{_instance_mutex};
    auto result = std::find(_buffers.begin(), _buffers.end(), nullptr);

    if (result == _buffers.end()) {
        return false;
    }

    ESP_LOGI("large_buffer_pool", "Got buffer %p back", buffer.get());
    *result = std::move(buffer);
    return true;

}