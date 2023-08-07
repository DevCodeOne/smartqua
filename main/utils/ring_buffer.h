#pragma once

#include <array>
#include <type_traits>

template <typename T, size_t N>
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
