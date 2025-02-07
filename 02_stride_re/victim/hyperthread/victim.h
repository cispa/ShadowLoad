#ifndef VICTIM_H
#define VICTIM_H

#include <stdint.h>
#include <sys/mman.h>
#include <pthread.h>

#include "timing.h"
#include "uarch.h"

#define VICTIM_BUFFER_SIZE (PAGE_SIZE * 10)

static uint8_t* victim_buffer = NULL;

#if defined(VICTIM_BUFFER_ADDRESS) || defined(VICTIM_GADGET_ADDRESS)

static uint8_t* map_buffer(uintptr_t address, uint64_t size);

#ifdef VICTIM_GADGET_ADDRESS
typedef void (*load_gadget_f)(void*);

static load_gadget_f map_load_gadget(uintptr_t address);
#endif /* VICTIM_GADGET_ADDRESS */

#endif /* VICTIM_BUFFER_ADDRESS || VICTIM_GADGET_ADDRESS */ 


#ifdef VICTIM_GADGET_ADDRESS
    load_gadget_f _victim_gadget;
#else
    void _victim_gadget(void*);
#endif /* VICTIM_GADGET_ADDRESS */

void* thread_receiver();

static int victim_init(void) {
    #ifdef VICTIM_GADGET_ADDRESS
    _victim_gadget = map_load_gadget(VICTIM_GADGET_ADDRESS);
    if(!_victim_gadget) {
        return -1;
    }
    #endif /* VICTIM_GADGET_ADDRESS */
    
    #ifdef VICTIM_BUFFER_ADDRESS
    victim_buffer = map_buffer(VICTIM_BUFFER_ADDRESS, VICTIM_BUFFER_SIZE);
    if(!victim_buffer) {
        return -1;
    }
    #else
    victim_buffer = mmap(NULL, VICTIM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    if(victim_buffer == MAP_FAILED) {
        ERROR("failed to map victim buffer!\n");
        return -1;
    }
    #endif /* VICTIM_BUFFER_ADDRESS */
    DEBUG("victim buffer: %p\n", victim_buffer);
    DEBUG("victim load gadget: %p\n", _victim_gadget);

    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(THREAD_CORE, &cpuset);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

    if (pthread_create(&thread, &attr, thread_receiver, NULL) != 0) {
        ERROR("Couldn't create thread");
        return -1;
    }

    return 0;
}

static uintptr_t victim_buffer_address(void) {
    #ifdef VICTIM_BUFFER_ADDRESS
        return VICTIM_BUFFER_ADDRESS;
    #else
        return (uintptr_t) victim_buffer;
    #endif /* VICTIM_BUFFER_ADDRESS */
}

static uintptr_t victim_load_address(void) {
    #ifdef VICTIM_GADGET_ADDRESS
        return VICTIM_GADGET_ADDRESS;
    #else
        return (uintptr_t) _victim_gadget;
    #endif /* VICTIM_GADGET_ADDRESS */
}

static inline __attribute__((always_inline)) uint64_t victim_probe_hyperthread(uint64_t offset) {
    register uint64_t start, end;
    mfence();
    start = timestamp();
    mfence();
    maccess(&victim_buffer[offset]);
    mfence();
    end = timestamp();
    mfence();  
    return end - start;
}

volatile int probe_now = 0;
volatile uint64_t probe_offset = 0;
volatile int probe_finished = 0;
volatile uint64_t probe_result = 0;

static uint64_t victim_probe(uint64_t offset) {
    probe_offset = offset;
    mfence();
    probe_now = 1;
    while (1) {
        if (probe_finished) {
            probe_finished = 0;
            return probe_result;
        }
    }
}

static void victim_flush_buffer(void) {
    for(uint64_t offset = 0; offset < VICTIM_BUFFER_SIZE; offset += CACHE_LINE_SIZE) {
        flush(&victim_buffer[offset]);
    }
}

#ifdef VICTIM_GADGET_ADDRESS
#define victim_load_gadget_hyperthread(offset) _victim_gadget(&victim_buffer[offset])
#else
static __attribute__((noinline, naked)) void victim_load_gadget_hyperthread(uint64_t offset) {
    _maccess(
        ".global _victim_gadget\n"
        "_victim_gadget:\n",
        &victim_buffer[offset]
    );
    asm volatile(return_asm());
}
#endif /* VICTIM_GADGET_ADDRESS */

volatile int load_gadget_now = 0;
volatile uint64_t load_gadget_offset = 0;
volatile int load_gadget_finished = 0;

static void victim_load_gadget(uint64_t offset) {
    load_gadget_offset = offset;
    mfence();
    load_gadget_now = 1;
    while (1) {
        if (load_gadget_finished) {
            load_gadget_finished = 0;
            return;
        }
    }
}

void* thread_receiver() {
    while (1) {
        if (probe_now) {
            probe_now = 0;

            probe_result = victim_probe_hyperthread(probe_offset);
            mfence();

            probe_finished = 1;
        }
        if (load_gadget_now) {
            load_gadget_now = 0;

            victim_load_gadget_hyperthread(load_gadget_offset);

            load_gadget_finished = 1;
        }
    }
}

void victim_destroy(void) {
    munmap(victim_buffer, VICTIM_BUFFER_SIZE);
    #ifdef VICTIM_GADGET_ADDRESS
    munmap(_victim_gadget, 2 * PAGE_SIZE);
    #endif /* VICTIM_GADGET_ADDRESS */
}

#endif /* VICTIM_H */
