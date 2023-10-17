//
// Created by asus on 7/26/2021.
//

#include <cstdlib>
#include <time.h>
#include <cstdio>
#include "../exp-backoff/include/backoff_algorithm.h"
#include "ExpBackoff.h"

ExpBackoff::ExpBackoff(uint32_t max_attempts, uint16_t max_delay_ms, uint16_t base_delay_ms) {
    struct timespec tp;
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_sec);

    BackoffAlgorithm_InitializeParams(&retryParams,
                                      base_delay_ms,
                                      max_delay_ms,
                                      max_attempts);
}

uint16_t ExpBackoff::get_backoffValue() {
    BackoffAlgorithmStatus_t retryStatus = BackoffAlgorithmSuccess;
    uint16_t nextRetryBackoff = 0;
    retryStatus = BackoffAlgorithm_GetNextBackoff(&retryParams, rand(), &nextRetryBackoff);
    if(retryStatus != BackoffAlgorithmSuccess)
        return 0;
    return nextRetryBackoff;
}
