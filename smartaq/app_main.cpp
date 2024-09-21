#include <pthread.h>
#include <esp_log.h>

#include "main_thread.h"

unsigned int stack_size = 6 * 4096;

extern "C" void app_main(void) {
    pthread_attr_t attributes;
    pthread_t mainThreadHandle;

    pthread_attr_init(&attributes);

    //pthread_attr_setstack(&attributes, pthreadStack.get(), stackSize);
    // Only stack size can be set on esp-idf
    pthread_attr_setstacksize(&attributes, stack_size);
    int result = pthread_create(&mainThreadHandle, &attributes, mainTask, NULL);
    if (result != 0) {
        ESP_LOGE("Main", "Couldn't start main thread");
    }

    result = pthread_join(mainThreadHandle, NULL);
    if (result != 0) {
        ESP_LOGE("Main", "Main thread exited shouldn't happen");
    }

    pthread_attr_destroy(&attributes);
}
