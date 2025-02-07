#ifndef TIME_H
#define TIME_H

#include <x86intrin.h>

#define time_init() 0
#define time_destroy() ;
#define timestamp() __rdtsc()

#endif /* TIME_H */
