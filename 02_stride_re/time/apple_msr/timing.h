#ifndef TIME_H
#define TIME_H

#include <stdint.h>

#define time_init() 0
#define time_destroy() ;

// needs to be enabled by kernel module!
static inline __attribute__((always_inline)) uint64_t timestamp() {
    uint64_t value;
    asm volatile("mrs %0, s3_2_c15_c0_00" : "=r" (value));
    return value;
}

#endif /* TIME_H */
