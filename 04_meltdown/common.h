#ifndef COMMON_H
#define COMMON_H

/* only x86_64 supported for this evaluation. */

#define CACHE_LINE_SIZE 64

#define maccess(x) asm volatile("mov (%0), %%rax" :: "r" (x) : "rax")
#define mfence() asm volatile("mfence")
#define nop() asm volatile("nop")
#define flush(x) asm volatile("clflush (%0)" :: "r" (x));

static uint64_t seed = 42;

static uint64_t rand64(void){
    return (seed = (164603309694725029ull * seed) % 14738995463583502973ull);
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
    for(uint32_t i = 0; i < 1000000000; ++i) nop();
    for(uint32_t i = 0; i < 100; i++){
        vals[i] = probe(&vals[50]);
        mfence();
    }
    qsort(vals, 100, sizeof(uint64_t), compare_int64); 
    return vals[90] + 40;
}

static void flush_region(uintptr_t start, size_t length){
    length += start & (CACHE_LINE_SIZE - 1);
    start -= start & (CACHE_LINE_SIZE - 1);
    for(uintptr_t offset = 0; offset < length; offset += CACHE_LINE_SIZE){
        flush((uint8_t*)start + offset);
    }
}

#endif /* COMMON_H */
