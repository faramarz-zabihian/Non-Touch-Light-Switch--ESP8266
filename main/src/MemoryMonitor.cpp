#include <freertos.h>
#include <freertos/task.h>
#include <lwipopts.h>
#include <MonitorMemory.h>

//
// Created by asus on 7/28/2021.
//
unsigned int MonitorMemory::last_heap = 0;

unsigned int MonitorMemory::heap() {
    return esp_get_free_heap_size();
}

void MonitorMemory::update() {
    update((const char *) "");
}

void MonitorMemory::update(int stage) {
    char line[10];
    sprintf(line, "%d", stage);
    update(line);
}

int i = 0;

void MonitorMemory::update(const char *stage) {

    unsigned int stack = uxTaskGetStackHighWaterMark(handle);
    unsigned int heap = esp_get_free_heap_size();
    bool dirty1 = false;
    bool dirty2 = false;
    if (stack != last_stack) {
        last_stack = stack;
        dirty1 = true;
    }
    if (std::abs((long) (heap - last_heap)) > 500) {
        last_heap = heap;
        dirty2 = true;
    }
    if (dirty1 || dirty2) {
        printf("%s-%s: ", name, stage);
        if (dirty1)
            printf("stack(%u)\t", stack);
        if (dirty2)
            printf("%sheap(%u)",  dirty1 ? "" : "\t\t", heap);
        puts("");
    }
}


MonitorMemory::MonitorMemory(
        const char *name) : name(name) {
    handle = xTaskGetCurrentTaskHandle();
};
