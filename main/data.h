#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

enum struct data_result { timed_out, successfull };

template <typename T>
class shared_data final {
   public:
    shared_data();
    ~shared_data();

    data_result get_data(T *data, uint32_t timeout = 10) const;
    data_result write_data(const T *data, uint32_t timeout = 10);

   private:
    T m_data{};
    SemaphoreHandle_t m_semaphore = nullptr;
};

template <typename T>
shared_data<T>::shared_data() : m_semaphore(xSemaphoreCreateRecursiveMutex()) {}

template <typename T>
shared_data<T>::~shared_data() {
    vSemaphoreDelete(m_semaphore);
}

template <typename T>
data_result shared_data<T>::get_data(T *data, uint32_t timeout) const {
    if (xSemaphoreTakeRecursive(m_semaphore, (TickType_t)timeout) != pdTRUE) {
        return data_result::timed_out;
    }

    *data = m_data;

    xSemaphoreGiveRecursive(m_semaphore);

    return data_result::successfull;
}

template <typename T>
data_result shared_data<T>::write_data(const T *data, uint32_t timeout) {
    if (xSemaphoreTakeRecursive(m_semaphore, (TickType_t)timeout) != pdTRUE) {
        return data_result::timed_out;
    }

    m_data = *data;

    xSemaphoreGiveRecursive(m_semaphore);
    return data_result::successfull;
}
