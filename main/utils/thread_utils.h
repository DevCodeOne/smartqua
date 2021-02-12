#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class thread_creator {
    public:
        template<typename Func>
        static void create_thread(Func func, const char *task_name, void *ptr = nullptr) {
            xTaskCreatePinnedToCore(func, "mainTask", 4096, ptr, 5, nullptr, 0);
        }
};