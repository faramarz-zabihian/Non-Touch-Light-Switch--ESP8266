//
// Created by asus on 7/28/2021.
//

#ifndef APP_MONITORMEMORY_H
#define APP_MONITORMEMORY_H
#include "freertos/task.h"
class MonitorMemory {
private:
    unsigned int last_stack = 0;
    static unsigned int last_heap;
    char const *name = nullptr;
    TaskHandle_t handle;
public :
    explicit MonitorMemory(char const *name);
    static unsigned int heap();
    void update();
    void update(int stage);
    void update(const char *stage);


};
#endif //APP_MONITORMEMORY_H
