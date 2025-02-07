#ifndef TIME_H
#define TIME_H

#include <pthread.h>
#include <stdint.h>

static pthread_t counter_thread;

static int counters_active = 0;

static uint64_t counter_val = 0;

static void* _counter_thread_thread(void*) {
    while(1) {
        ++counter_val;
    }
}

static int time_init() {
    if(!(counters_active ++)) {
        return pthread_create(&counter_thread, NULL, _counter_thread_thread, NULL);
    }
    return 0;
}

static void time_destroy() {
    if(!(--counters_active)) {
        pthread_cancel(counter_thread);
    }
}

#define timestamp() counter_val

#endif /* TIME_H */
