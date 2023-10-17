//
// Created by asus on 7/26/2021.
//

#ifndef HELLO_WORLD_EXPBACKOFF_H
#define HELLO_WORLD_EXPBACKOFF_H

#endif //HELLO_WORLD_EXPBACKOFF_H
#include "../exp-backoff/include/backoff_algorithm.h"

class ExpBackoff {
    BackoffAlgorithmContext_t retryParams;
public:
    ExpBackoff(uint32_t max_attempts, uint16_t max_delay_ms, uint16_t base_delay_ms);
    uint16_t get_backoffValue();
};