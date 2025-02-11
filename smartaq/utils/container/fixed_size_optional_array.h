#pragma once

#include <array>
#include <optional>
#include <ranges>
#include <shared_mutex>

template<typename T, std::size_t Size>
/**
 * \brief A fixed-size optional array container.
 *
 * This class provides a container of fixed size where each element
 * is an optional value. It provides utilities for managing and
 * accessing the array elements, ensuring flexible storage and retrieval.
 */
class FixedSizeOptionalArray {
public:
    using InnerContainerType = std::array<std::optional<T>, Size>;
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
        return count() == 0;
    }

    /**
     * \brief Clear the array.
     */
    void clear() noexcept {
        for (auto &elem: mData) {
            elem.reset();
        }
    }

    /**
     * \brief Determines whether a specified element exists within the container.
     * \param value The element to locate within the container.
     * \return True if the specified element exists in the container, false otherwise.
    */
    [[nodiscard]] bool contains(const T &value) const {
        return std::ranges::find_if(
                   mData,
                   [&](const std::optional<T> &elem) { return elem.has_value() && elem.value() == value; }
               ) != mData.end();
    }

    /**
     * \brief Insert an element at the specified index.
     * \param index The index at which to insert the element.
     * \param value The value to insert.
     */
    void insert(std::size_t index, const T &value) {
        mData[index] = value;
    }

    /**
     * \brief Erase an element at the specified index.
     * \param index The index of the element to erase.
     */
    void erase(std::size_t index) {
        mData[index].reset();
    }

    /**
     * \brief Get the number of elements currently stored.
     * \return The number of elements currently stored.
     */
    [[nodiscard]] std::size_t count() const noexcept {
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
        auto it = std::ranges::find_if(mData, [](auto &elem) { return !elem.has_value(); });

        if (it != mData.end()) {
            *it = value;
            return true;
        }
        return false;
    }

    /**
     * \brief Removes all occurrences of the specified value from the collection
     * \param value The value to be removed from the collection.
     * \return Returns true if the value was successfully removed, false if the value was not found.
     */
    [[nodiscard]] bool removeValue(const T &value) {
        bool removedValue = false;
        for (auto &currentValue : mData) {
            if (currentValue.has_value() && currentValue.value() == value) {
                removedValue = true;
                currentValue.reset();
            }
        }
        return removedValue;
    }


    /**
     * \brief Remove elements from the container if a predicate is satisfied.
     * \param predicate A callable object that takes an element as input and returns true if the condition is met, false otherwise.
     * \return True if one, or more elements satisfying the predicate were found and removed, false otherwise.
     */
    template<typename Predicate>
    [[nodiscard]] bool modifyOrRemove(Predicate predicate) {
        bool hasRemovedElement = false;
        for (auto &currentElement : mData) {
            if (currentElement.has_value() && predicate(currentElement.value())) {
                currentElement.reset();
                hasRemovedElement = true;
            }
        }

        return hasRemovedElement;
    }

    [[nodiscard]] bool full() const {
        return count() == Size;
    }
    /**
     * \brief Custom iterator for non-empty elements.
     */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        Iterator(typename std::array<std::optional<T>, Size>::iterator current,
                 typename std::array<std::optional<T>, Size>::iterator end)
            : mCurrent(current), mEnd(end) {
            // Move the iterator to the first non-empty element
            advanceIfEmpty();
        }

        reference operator*() const {
            return mCurrent->value();
        }

        pointer operator->() const {
            return &mCurrent->value();
        }

        Iterator &operator++() {
            ++mCurrent;
            advanceIfEmpty();
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const Iterator &a, const Iterator &b) {
            return a.mCurrent == b.mCurrent;
        }

        friend bool operator!=(const Iterator &a, const Iterator &b) {
            return !(a == b);
        }

    private:
        void advanceIfEmpty() {
            while (mCurrent != mEnd && !mCurrent->has_value()) {
                ++mCurrent;
            }
        }

        typename InnerContainerType::iterator mCurrent;
        typename InnerContainerType::iterator mEnd;
    };

    Iterator begin() {
        return Iterator(mData.begin(), mData.end());
    }

    Iterator end() {
        return Iterator(mData.end(), mData.end());
    }

    /**
     * \brief Get an iterator pointing to the first non-empty element.
     * \return An iterator to the first non-empty element, or end() if no such element is found.
     */
    Iterator findFirstNonEmpty() {
        return std::ranges::find_if(mData.begin(), mData.end(), [](const auto &elem) { return elem.has_value(); });
    }

    /**
     * \brief Get an iterator pointing to the last non-empty element.
     * \return An iterator to the last non-empty element, or end() if no such element is found.
     */
    Iterator findLastNonEmpty() {
        auto it = std::ranges::find_if(mData.rbegin(), mData.rend(), [](const auto &elem) { return elem.has_value(); });
        if (it != mData.rend()) {
            return Iterator(it.base() - 1, mData.end());
        }
        return end();
    }

private:
    InnerContainerType mData;
};
