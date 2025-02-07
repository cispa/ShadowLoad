#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096

typedef void (*load_gadget_f)(void*);

#define maccess(x) asm volatile("mov (%0), %%al" :: "r" (x) : "rax")
#define mfence() asm volatile("mfence")
#define nop() asm volatile("nop")
#define flush(x) asm volatile("clflush (%0)" :: "r" (x));

static uint8_t* map_buffer(uintptr_t address, uint64_t size) {
    void* mapping = mmap((void*)address, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED_NOREPLACE, -1, 0);
    if(mapping == MAP_FAILED) {
        return NULL;
    }
    if(mapping != (void*)address) {
        munmap(mapping, size);
        return NULL;
    }
    return mapping;
}

// defined in gadget.S
extern void load_gadget_start();
extern void load_gadget_end();

static load_gadget_f map_gadget(uintptr_t address) {
    uint8_t* mapping = map_buffer(address - (address % PAGE_SIZE), 2 * PAGE_SIZE);
    if(!mapping){
        return NULL;
    }
    memcpy((void*)address, load_gadget_start, (uintptr_t)load_gadget_end - (uintptr_t)load_gadget_start);
    mprotect(mapping, 2 * PAGE_SIZE, PROT_READ | PROT_EXEC);
    mapping += address % PAGE_SIZE;
    return (load_gadget_f)(void*) mapping;
}

static inline __attribute__((always_inline)) uint64_t _rdtsc(void) {
    uint64_t a, d;
    mfence();
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    a = (d << 32) | a;
    mfence();
    return a;
}

static int compare_int64(const void * a, const void * b) {
    return *(int64_t*)a - *(int64_t*)b;
}

static inline __attribute__((always_inline)) uint64_t probe(void* addr){
    uint64_t start, end;
    start = _rdtsc();
    maccess(addr);
    end = _rdtsc();
    return end - start;
}

static uint64_t calculate_threshold(){
    uint64_t vals[100];
    for(uint32_t i = 0; i < 10000000; ++i) nop();
    for(uint32_t i = 0; i < 100; i++){
        vals[i] = probe(&vals[50]);
        mfence();
    }
    qsort(vals, 100, sizeof(uint64_t), compare_int64); 
    return vals[90] + 40;
}

#endif /* COMMON_H */
