#pragma once

#include <array>
#include <optional>
#include <ranges>
#include <shared_mutex>

template<std::size_t Size, typename T>
class FixedSizeOptionalArray {
public:
    /**
     * \brief Default constructor.
     */
    FixedSizeOptionalArray() = default;

    /**
     * \brief Access element at index without bounds checking.
     * \param index The index of the element to access.
     * \return A reference to the optional element at the specified index.
     */
    std::optional<T> &operator[](std::size_t index) {
        std::shared_lock lock(mMtx);
        return mData[index];
    }

    /**
     * \brief Get the size of the array.
     * \return The size of the array.
     */
    [[nodiscard]] static constexpr std::size_t size() noexcept {
        return Size;
    }

    // Check if the array is empty
    [[nodiscard]] bool empty() const noexcept {
        std::shared_lock lock(mMtx);
        return count() == 0;
    }

    /**
     * \brief Clear the array.
     */
    void clear() noexcept {
        std::unique_lock lock(mMtx);
        for (auto &elem: mData) {
            elem.reset();
        }
    }

    /**
     * \brief Insert an element at the specified index.
     * \param index The index at which to insert the element.
     * \param value The value to insert.
     */
    void insert(std::size_t index, const T &value) {
        std::unique_lock lock(mMtx);
        mData[index] = value;
    }

    /**
     * \brief Erase an element at the specified index.
     * \param index The index of the element to erase.
     */
    void erase(std::size_t index) {
        std::unique_lock lock(mMtx);
        mData[index].reset();
    }

    /**
     * \brief Get the number of elements currently stored.
     * \return The number of elements currently stored.
     */
    [[nodiscard]] std::size_t count() const noexcept {
        std::shared_lock lock(mMtx);
        std::size_t cnt = 0;
        for (const auto &elem: mData) {
            if (elem.has_value()) {
                ++cnt;
            }
        }
        return cnt;
    }

    /**
     * \brief Append an element at the first empty optional.
     * \param value The value to append.
     * \return True if the element was appended, false otherwise.
     */
    [[nodiscard]] bool append(const T &value) {
        std::unique_lock lock(mMtx);
        auto it = std::ranges::find_if(mData, [](auto &elem) { return !elem.has_value(); });

        if (it != mData.end()) {
            *it = value;
            return true;
        }
        return false;
    }

private:
    std::array<std::optional<T>, Size> mData;
    mutable std::shared_mutex mMtx;
};
