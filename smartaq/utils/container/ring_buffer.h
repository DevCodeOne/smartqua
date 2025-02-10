#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <type_traits>

template <typename T, size_t N>
requires (N > 1)
class RingBuffer {
   public:
    using SizeType = decltype(N);

    T &operator[](SizeType index) {
        return m_data[calculateRealIndex(index)];
    }

    const T &operator[](SizeType index) const {
        return m_data[calculateRealIndex(index)];
    }

    RingBuffer &append(T value) {
        if (m_size == m_data.size()) {
            // Wrap around, size isn't increased
            m_data[calculateRealIndex(0)] = value;
            m_offset = (m_offset + 1) % m_data.size();
        } else {
            m_data[calculateRealIndex(m_size)] = value;
            ++m_size;
        }
        return *this;
    }

    [[nodiscard]] bool empty() const {
        return m_size == 0;
    }

    void removeFront() {
        if (empty()) {
            return;
        }
        m_offset = (m_offset + 1) % m_data.size();
        --m_size;
    }

    void removeBack() {
        if (empty()) {
            return;
        }
        --m_size;
    }

    [[nodiscard]] std::optional<T> takeBack() {
        if (empty()) {
            return std::nullopt;
        }
        auto value = m_data[calculateRealIndex(m_size - 1)];
        removeBack();
        return { std::move(value) };
    }

    [[nodiscard]] std::optional<T> takeFront() {
        if (empty()) {
            return std::nullopt;
        }
        auto value = m_data[calculateRealIndex(0)];
        removeFront();
        return { std::move(value) };
    }

    T &front() { return operator[](0); }

    const T &front() const { return operator[](0); }

    T &back() { return operator[](size() - 1); }

    const T &back() const { return operator[](size() - 1); }

    SizeType size() const { return m_size; }

   private:
    SizeType calculateRealIndex(SizeType n) const {
        return (m_offset + n) % m_data.size();
    }

    std::array<T, N> m_data;
    SizeType m_offset = 0;
    SizeType m_size = 0;
};

template<typename T, size_t N>
requires (N > 1)
class RingBufferIterator {
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = std::add_pointer_t<T>;
    using reference = std::add_lvalue_reference_t<T>;
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;

    RingBufferIterator() = default;
    RingBufferIterator(T *base, size_t pos, size_t offset, size_t size) : size(size), pos(pos), offset(offset), base(base) {}
    RingBufferIterator(const RingBufferIterator &other) = default;
    RingBufferIterator(RingBufferIterator &&other) = default;
    ~RingBufferIterator() = default;

    RingBufferIterator &operator=(const RingBufferIterator &other) = default;
    RingBufferIterator &operator=(RingBufferIterator &&other) = default;

    const T &operator *() const {
        return *(base + offset);
    }

    T &operator *() {
        return *(base + offset);
    }

    RingBufferIterator &operator++() {
        pos = (pos + 1) % size;

        return *this;
    }

    RingBufferIterator &operator++(int) {
        auto old = *this;
        ++*this;

        return old;
    }

    bool operator==(const RingBufferIterator<T, N> &other) {
        return pos == other.pos 
            && base == other.base
            && size == other.size
            && offset == other.offset;
    }

    bool operator!=(const RingBuffer<T, N> &other) {
        return !this->operator==(other);
    }

    size_t size = 0;
    size_t pos = 0;
    size_t offset = 0;
    T *base = nullptr;
};