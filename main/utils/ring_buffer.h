#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <type_traits>

template <typename T, size_t N>
requires (N > 1)
class ring_buffer {
   public:
    static_assert(N > 0, "N has to be an unsigned type");

    using size_type = decltype(N);

    T &operator[](size_type index) {
        return const_cast<T &>(
            const_cast<const ring_buffer *>(this)->operator[](index));
    }

    const T &operator[](size_type index) const {
        return m_data[calc_real_index(index)];
    }

    ring_buffer &append(T value) { 
        m_data[calc_real_index(m_size)] = value; 
        if (m_size + 1 == m_data.size()) {
            // Wrap around, size isn't increased
            m_offset = (m_offset + 1) % m_data.size();
        } else {
            ++m_size;
        }
        return *this;
    }

    // TODO: implement
    // ring_buffer &pop(T &out) { }

    T &front() { return operator[](calc_real_index(0)); }

    const T &front() const { return operator[](calc_real_index(0)); }

    T &back() { return operator[](calc_real_index(size() - 1)); }

    const T &back() const { return operator[](calc_real_index(size() - 1)); }

    size_type size() const { return m_size; }

   private:
    size_type calc_real_index(size_type n) const { return (m_offset + n) % m_data.size(); }

    std::array<T, N> m_data;
    size_type m_offset = 0;
    size_type m_size = 0;
};

template<typename T, size_t N>
requires (N > 1)
class ring_buffer_iterator {
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = std::add_pointer_t<T>;
    using reference = std::add_lvalue_reference_t<T>;
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;

    ring_buffer_iterator() = default;
    ring_buffer_iterator(T *base, size_t pos, size_t offset, size_t size) : size(size), pos(pos), offset(offset), base(base) {}
    ring_buffer_iterator(const ring_buffer_iterator &other) = default;
    ring_buffer_iterator(ring_buffer_iterator &&other) = default;
    ~ring_buffer_iterator() = default;

    ring_buffer_iterator &operator=(const ring_buffer_iterator &other) = default;
    ring_buffer_iterator &operator=(ring_buffer_iterator &&other) = default;

    const T &operator *() const {
        return *(base + offset);
    }

    T &operator *() {
        return *(base + offset);
    }

    ring_buffer_iterator &operator++() {
        pos = (pos + 1) % size;

        return *this;
    }

    ring_buffer_iterator &operator++(int) {
        auto old = *this;
        ++*this;

        return old;
    }

    bool operator==(const ring_buffer_iterator<T, N> &other) {
        return pos == other.pos 
            && base == other.base
            && size == other.size
            && offset == other.offset;
    }

    bool operator!=(const ring_buffer<T, N> &other) {
        return !this->operator==(other);
    }

    size_t size = 0;
    size_t pos = 0;
    size_t offset = 0;
    T *base = nullptr;
};