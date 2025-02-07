#ifndef TIME_H
#define TIME_H

#include <stdint.h>

#define time_init() 0
#define time_destroy() ;

static inline __attribute__((always_inline)) uint64_t timestamp() {
    uint64_t value;
    asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (value));
    return value;
}

#endif /* TIME_H */
