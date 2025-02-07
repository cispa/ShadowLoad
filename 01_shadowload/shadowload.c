#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

void load_gadget_start(void* address);
void load_gadget_end(void);

#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64

/* architecture dependent code */
#if defined(__x86_64__)
#define maccess(x) asm volatile("mov (%0), %%rax" :: "r" (x) : "rax")
#define nop() asm volatile("nop")
#define mfence() asm volatile("mfence")
#define flush(x) asm volatile("clflush (%0)" :: "r" (x));
#ifdef APERF 
    static inline __attribute__((always_inline)) uint64_t _rdtsc(void) {
      uint64_t a, d;
      asm volatile("mfence");
      asm volatile("rdpru" : "=a" (a), "=d" (d) : "c" (1));
      a = (d << 32) | a;
      asm volatile("mfence");
      return a;
    }
#else // use rdtsc
static inline __attribute__((always_inline)) uint64_t _rdtsc(void) {
    uint64_t a, d;
    mfence();
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    a = (d << 32) | a;
    mfence();
    return a;
}
#endif /* APERF */
#elif defined (__aarch64__)
#define maccess(x) asm volatile("ldr x0, [%0]" :: "r" (x) : "x0")
#define nop() asm volatile("nop")
#define mfence() asm volatile("DSB SY\nISB")
static inline __attribute__((always_inline)) uint64_t _rdtsc(void) {
    uint64_t a;
    mfence();
    asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (a));
    mfence();
    return a;
}
static uint8_t evict_buffer[PAGE_SIZE * 1000];

#endif /* ARCHITECTURE */

#define VICTIM_BUFFER_SIZE (PAGE_SIZE * 5)

// measure time of memory load
static inline __attribute__((always_inline)) uint64_t probe(void* addr){
    uint64_t start, end;
    start = _rdtsc();
    maccess(addr);
    end = _rdtsc();
    return end - start;
}
    
// buffer that collides with victim buffer
uint8_t* colliding_buffer;
    
// load instruction that collides with load gadget in victim
void (*colliding_load)(void*);

#ifdef KERNEL_MODULE

#include <fcntl.h>
#include <sys/ioctl.h>
#include "kernel_module/shadowload_module.h"

int module_fd;

static void flush_victim_buffer(void) {
    ioctl(module_fd, CMD_FLUSH, 0);
    
    #if defined (__x86_64__)
    #ifdef FLUSH_COLLIDING
    for(uint64_t offset = 0; offset < VICTIM_BUFFER_SIZE; offset += CACHE_LINE_SIZE) {
        flush(&colliding_buffer[offset]);
    }
    #endif /* FLUSH_COLLIDING */
    #endif /* __x86_64__ */
}

static uint64_t probe_victim_buffer(uint64_t offset) {
    uint64_t io = offset;
    ioctl(module_fd, CMD_PROBE, &io);
    return io;
}

static void load_gadget(uint64_t offset) {
    ioctl(module_fd, CMD_GADGET, offset);
}

#elif defined (SGX)

#include "sgx_enclave/sgx_victim.h"

#define flush_victim_buffer sgx_flush_victim_buffer
#define probe_victim_buffer sgx_probe_victim_buffer
#define load_gadget sgx_load_gadget

#else

static uint8_t* victim_buffer;

static void flush_victim_buffer(void) {
    #if defined (__aarch64__)
    // no flushing available across all arm devices in userspace. Just use eviction
    for(int i = 0; i < 3; i++) {
        for(uint64_t offset = 0; offset < sizeof(evict_buffer); offset += 64) {
            maccess(&evict_buffer[offset]);
        }
    }
    
    #else // x86_64
    for(uint64_t offset = 0; offset < VICTIM_BUFFER_SIZE; offset += CACHE_LINE_SIZE) {
        flush(&victim_buffer[offset]);
        #ifdef FLUSH_COLLIDING
        flush(&colliding_buffer[offset]);
        #endif /* FLUSH_COLLIDING */
    }
    #endif /* __aarch64__ */
}

static uint64_t probe_victim_buffer(uint64_t offset) {
    return probe(&victim_buffer[offset]);
}

static void load_gadget(uint64_t offset) {
    load_gadget_start(&victim_buffer[offset]);
}

#endif /* KERNEL_MODULE */

static int compare_int64(const void * a, const void * b) {
    return *(int64_t*)a - *(int64_t*)b;
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

static uint64_t shadowload(uint64_t stride, int accesses, int aligned) {
     
    uint64_t victim_offset = aligned ? accesses * stride : 0;
    
    flush_victim_buffer();
    
    // this is required. Not 100% sure why, but without nopping, there is no prefetching
    for(int i = 0; i < 10000000; i++) nop();
    
    // repeating 5 times is not necessary, but there is no reason not to (and it may increase chance of success)
    for(int repeat = 0; repeat < 5; repeat ++) {
        
        for(int access = 0; access < accesses; access++) {
            colliding_load(&colliding_buffer[access * stride]);
            mfence();
        }
        
        load_gadget(victim_offset);    
        mfence();
    }
    
    return probe_victim_buffer(victim_offset + stride);
}


int main(int argc, char** argv) {
    
    // get processor into steady state
    for(int i = 0; i < 100000000; i++) nop();
    
    // address of load that collides with victim load
    uintptr_t colliding_load_address;
    
    // address of buffer that collides with victim buffer
    uintptr_t colliding_buffer_address;
    
    // threshold to distinquish cache hits from misses
    uint64_t threshold = calculate_threshold();
    
    
    #ifdef KERNEL_MODULE
    
    struct shadowload_kernel_info info;
    
    module_fd = open(SHADOWLOAD_MODULE_DEVICE_PATH, O_RDONLY);
    
    if(module_fd < 0) {
        fputs("unable to open module!\n", stderr);
        return -1;
    }
    
    // get required information for kernel
    ioctl(module_fd, CMD_INFO, &info);
    
    colliding_load_address = info.kernel_access & 0x7fffffffffffull;
    colliding_buffer_address  = info.kernel_buffer & 0x7fffffffffffull;
    
    #elif defined (SGX)
    
    threshold = 150;

    if(sgx_start()){
        fputs("failed to start SGX victim!\n", stderr);
        return -1;
    }
    
    sgx_get_info((void**)(void*)&colliding_load_address, (void**)(void*)&colliding_buffer_address);
    colliding_load_address ^= 1ull << 46ull;
    colliding_buffer_address ^= 1ull << 46ull;
    
    #else
    
    // map buffer in userspace (at any arbitrary address)
    victim_buffer = mmap(NULL, VICTIM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    if(victim_buffer == MAP_FAILED){
        fputs("unable to allocate buffer!", stderr);
        return -1;
    }
    
    // colliding buffer address is address of buffer but bit 46 flipped
    colliding_buffer_address = (uintptr_t)victim_buffer ^ (1ull << 46ull);
    
    // use address of gadget but flip bit 46
    colliding_load_address = (uintptr_t)load_gadget_start ^ (1ull << 46ull);
    
    #endif /* KERNEL_MODULE */
    
    // map colliding memory buffer
    colliding_buffer = mmap((void*) colliding_buffer_address, VICTIM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED_NOREPLACE, -1, 0);
    if(colliding_buffer == MAP_FAILED) {
        fprintf(stderr, "failed to map colliding memory buffer to 0x%016zx\n", colliding_buffer_address);
        return -1;
    }
    
    // map colliding memory access instruction
    uint8_t* code_buf = mmap((void*)(colliding_load_address & 0x7ffffffff000ull), PAGE_SIZE * 2, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED_NOREPLACE, -1, 0);
    if(code_buf == MAP_FAILED) {
        fprintf(stderr, "unable to allocate colliding memory load to 0x%016zx\n", (uint64_t)(colliding_load_address & 0x7ffffffff000ull));
        return -1;
    }
    memcpy(code_buf + (colliding_load_address & 0xfff), load_gadget_start, (uint8_t*)load_gadget_end - (uint8_t*)load_gadget_start);
    mprotect(code_buf, PAGE_SIZE * 2, PROT_READ | PROT_EXEC);
    colliding_load = (void*)(code_buf + (colliding_load_address & 0xfff));
    
//    printf("colliding buffer: %p\ncolliding load: %p\n", colliding_buffer, colliding_load);

    for(int accesses = 1; accesses <= 8; accesses ++) {
        for(uint64_t stride = 64; stride <= 2048; stride += 64) {
            for(int aligned = 0; aligned <= 1; aligned ++) {
                int hits = 0;
                for(int repeat = 0; repeat < 100; repeat ++) {
                    hits += shadowload(stride, accesses, aligned) < threshold;
                }
                printf("%d %zu %d %d\n", accesses, stride, aligned, hits);
            }
        }
    }
    
    #ifdef SGX
    sgx_stop();
    #endif /* SGX */
    
}
