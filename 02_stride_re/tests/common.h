#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>

#include "log.h"
#include "victim.h"
#include "uarch.h"
#include "timing.h"

#define NOP_COUNT 100000

// this code must be in a header file and not C file, otherwise, the victim would be instanciated twice.
// The victim also has to be in a header file as we want to make use of inlining, etc.

static uint64_t seed = 42;

static uint64_t rand64(void){
    return (seed = (164603309694725029ull * seed) % 14738995463583502973ull);
}


typedef void (*load_gadget_f)(void*);

static uint64_t calculate_threshold(void) {
    
    /* bring processor into steady state */
    
    for(int i = 0; i < 1000000000; i++) nop();
    
    
    /* measure cache hits and misses */
    
    // offset for measuring cache hits / misses
    uint64_t offset = VICTIM_BUFFER_SIZE / 2;
    
    // bring cache line into cache
    victim_probe(offset);
    
    // measure 1000 cache hits (but ignore outliers)
    uint64_t cache_hit_sum = 0;
    for(int measured = 0; measured < 1000;){
        uint64_t time = victim_probe(offset);
        if(time && time < 1000) {
            cache_hit_sum += time;
            measured ++;
        }
    }
    
    // measure 1000 cache misses (but ignore outliers)
    uint64_t cache_miss_sum = 0;
    for(int measured = 0; measured < 1000;){
        // remove cache line from cache
        victim_flush_buffer();
        mfence();
     
        uint64_t time = victim_probe(offset);
        if(time && time < 1000) {
            cache_miss_sum += time;
            measured ++;
        }
    }
    
    // average cache hit and cache miss
    uint64_t avg_hit = cache_hit_sum / 1000;
    uint64_t avg_miss = cache_miss_sum / 1000;
    
    // make sure measurements work
    if(avg_hit >= avg_miss) {
        FATAL("invalid measurements: average hit: %zu, average miss: %zu\n", avg_hit, avg_miss);
    }
    
    DEBUG("cache hit  %zu\n", avg_hit);
    DEBUG("cache miss %zu\n", avg_miss);
    
    
    /* calculate threshold */
    
    // set threshold closer to cache hit
    uint64_t threshold = avg_hit + (avg_miss - avg_hit) / 5;
    // however, make sure it is above cache hit (for lower frequency timers, the thresholds may be very close)
    if(threshold == avg_hit) {
        threshold ++;
    }
    DEBUG("threshold  %zu\n", threshold);
    
    
    
    /* sanity check the threshold */
    
    // bring cache line into cache
    victim_probe(offset);
    
    // measure 1000 cache hits
    int hits = 0;
    for(int measured = 0; measured < 1000; measured ++){
        hits += victim_probe(offset) < threshold;
    }
    
    // measure 1000 cache misses
    int misses = 0;
    for(int measured = 0; measured < 1000; measured ++){
        // remove cache line from cache
        victim_flush_buffer();
        mfence();
     
        misses += victim_probe(offset) >= threshold;
    }
    
    DEBUG("sanity check hits   %d / 1000\n", hits);
    DEBUG("sanity check misses %d / 1000\n", misses);
    
    /* return threshold */
    return threshold;
}


static uint8_t* map_buffer(uintptr_t address, uint64_t size) {
    if(address % PAGE_SIZE) {
        ERROR("buffer is not page aligned: 0x%016zx (size: %zu)\n", address, size);
        return NULL;
    }
    if(size % PAGE_SIZE) {
        WARN("size is not page aligned: %zu (buffer: 0x%016zx)\n", size, address);
        size -= size % PAGE_SIZE;
        size += PAGE_SIZE;
    }
    
    uint8_t* mapping = mmap((void*) address, size, PROT_READ | PROT_WRITE, MAP_FIXED_NOREPLACE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    if(mapping == MAP_FAILED) {
        ERROR("failed to map buffer: 0x%016zx (size: %zu)\n", address, size);
        return NULL;
    } else if(mapping != (void*) address) {
        ERROR("got wrong address for buffer: 0x%016zx (size: %zu) -> 0x%016zx\n", address, size, (uintptr_t) mapping);
        return NULL;
    }
    
    DEBUG("mapped buffer: 0x%016zx (size: %zu)\n", address, size);
    return mapping;
}

static load_gadget_f map_load_gadget(uintptr_t address) {
    uint8_t* code_buffer = map_buffer(address - (address % PAGE_SIZE), 2 * PAGE_SIZE);
    if(!code_buffer) {
        ERROR("could not map buffer for load gadget at 0x%016zx\n", address);
        return NULL;
    }
    
    memcpy(&code_buffer[address % PAGE_SIZE], _load_gadget_asm_start, (uintptr_t)_load_gadget_asm_end - (uintptr_t)_load_gadget_asm_start);
    mprotect(code_buffer, 2 * PAGE_SIZE, PROT_READ | PROT_EXEC);
    
    return (load_gadget_f)(void*) &code_buffer[address % PAGE_SIZE];
}

#endif /* __COMMON_H */
