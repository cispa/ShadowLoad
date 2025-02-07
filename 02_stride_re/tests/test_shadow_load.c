#include "tests/common.h"
#include <unistd.h>

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a > b ? b : a)

#ifdef ACCESS_MEMORY
    #define DUMMY_BUFFER_SIZE (PAGE_SIZE * 10)
    static uint8_t* dummy_buffer;
#endif /* ACCESS_MEMORY */

static uint8_t* colliding_buffer;
static load_gadget_f colliding_load;

static uint64_t prefetch_time = 0;
static uint64_t gadget_time = 0;

static uint64_t get_time_ns() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static uint64_t prefetch(int64_t stride, int accesses, int aligned, int flush_all) {
    
    uint64_t start = aligned ? 0 : 2 * stride;
    
    if(flush_all) {
        victim_flush_buffer();
    } else {
        victim_flush_single(aligned ? (accesses + 1) * stride : stride);
    }
    mfence();
    prefetch_time = get_time_ns();
    mfence();   
    
    for(register int k = 0; k < 2; k++) {
        for(register int i = 0; i < accesses; i++) {
            mfence();
            colliding_load(colliding_buffer + start + i * stride);
        }
    }
    
    mfence();
    gadget_time = get_time_ns();
    mfence();
    prefetch_time = gadget_time - prefetch_time;
    victim_load_gadget(aligned ? accesses * stride : 0);
    mfence();
    gadget_time = get_time_ns() - gadget_time;
        
    return victim_probe(aligned ? (accesses + 1) * stride : stride);
}


int main(int argc, char** argv) {

    DEBUG(
        "settings: USE_NOP=%d, USE_FENCE=%d ACCESS_MEMORY=%d\n", 
        #ifdef USE_NOP
            1
        #else
            0
        #endif /* USE_NOP */
        ,
        #ifdef USE_FENCE
            1
        #else
            0
        #endif /* USE_FENCE */
        ,
        #ifdef ACCESS_MEMORY
            1
        #else
            0
        #endif /* ACCESS_MEMORY */
    );
    
    if(argc != 10) {
        FATAL("usage %s <stride> <accesses> <aligned> <colliding_buffer_address_and> <colliding_buffer_address_xor> <colliding_load_address_and> <colliding_load_address_xor> <flush_all> <repeats>\n", argv[0]);
    }
    
    if(time_init()) {
        FATAL("failed to initialize timer!\n");
    }

    if(victim_init()) {
        FATAL("failed to initialize victim!\n");
    }
    
    int64_t stride = strtoll(argv[1], NULL, 0);
    int accesses = atoi(argv[2]);
    int aligned = atoi(argv[3]);
    uintptr_t colliding_buffer_address = (victim_buffer_address() & strtoull(argv[4], NULL, 0)) ^ strtoull(argv[5], NULL, 0);
    uintptr_t colliding_load_address = (victim_load_address() & strtoull(argv[6], NULL, 0)) ^ strtoull(argv[7], NULL, 0);
    int flush_all = atoi(argv[8]);
    int repeats = atoi(argv[9]);
    
    DEBUG("arguments: stride=%zu, accesses=%d, aligned=%d, colliding_buffer_address_and=0x%016zx colliding_buffer_address_or=0x%016zx colliding_load_address_and=0x%016zx colliding_load_address_xor=0x%016zx flush_all=%d\n", stride, accesses, aligned, strtoull(argv[6], NULL, 0), strtoull(argv[7], NULL, 0), strtoull(argv[8], NULL, 0), strtoull(argv[9], NULL, 0), flush_all);
    
    mfence();
    uint64_t setup_start = get_time_ns();
    mfence();
    
    colliding_buffer = map_buffer(colliding_buffer_address - (colliding_buffer_address % PAGE_SIZE), VICTIM_BUFFER_SIZE + PAGE_SIZE);
    if(!colliding_buffer) {
        munmap((void*) victim_buffer_address(), VICTIM_BUFFER_SIZE);
        
        colliding_buffer = map_buffer(colliding_buffer_address - VICTIM_BUFFER_SIZE, 3*VICTIM_BUFFER_SIZE + PAGE_SIZE);
        if(colliding_buffer == MAP_FAILED) {
            FATAL("hotfix for buffer failed\n");
        }
        colliding_buffer += VICTIM_BUFFER_SIZE;
        DEBUG("used hotfix to map buffer\n");
    }
    if(!colliding_buffer) {
        FATAL("could not map colliding buffer to 0x%016zx\n", colliding_buffer_address);
    }
    colliding_buffer += colliding_buffer_address % PAGE_SIZE;
    
    colliding_load = map_load_gadget(colliding_load_address);
    if(!colliding_load) {
        uint64_t load_size = (uintptr_t)_load_gadget_asm_end - (uintptr_t)_load_gadget_asm_start;
        
        if(load_size > MAX(colliding_load_address, victim_load_address()) - MIN(colliding_load_address, victim_load_address())) {
            FATAL("hotfix for load mapping with small differences failed: Gadgets would overlap!\n");
        }
        
        // force map. This may break things but try anyway
        uint8_t* buf = mmap((void*)(colliding_load_address - (colliding_load_address % PAGE_SIZE) - PAGE_SIZE * 2), 6 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
        if(buf == MAP_FAILED || buf != (void*)(colliding_load_address - (colliding_load_address % PAGE_SIZE) - PAGE_SIZE * 2)) {
            FATAL("hotfix for load mapping with small difference failed!\n");
        }
        
        buf += PAGE_SIZE * 2;
        
        memcpy((void*)victim_load_address(), _load_gadget_asm_start, load_size);
        
        memcpy(&buf[colliding_load_address % PAGE_SIZE], _load_gadget_asm_start, load_size);
        mprotect(buf - PAGE_SIZE * 2, 6 * PAGE_SIZE, PROT_EXEC | PROT_READ);
        
        colliding_load = (load_gadget_f)(void*) &buf[colliding_load_address % PAGE_SIZE];
        DEBUG("used hotfix to map load\n");
    }
    if(!colliding_load) {
        FATAL("could not map colliding load to 0x%016zx\n", colliding_load_address);
    }
    
    mfence();
    uint64_t setup_end = get_time_ns();
    mfence();
    RESULT("setup_time: %zu\n", setup_end - setup_start);
    
    
    #ifdef ACCESS_MEMORY
        dummy_buffer = mmap(NULL, DUMMY_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
        if(dummy_buffer == MAP_FAILED) {
            FATAL("failed to map memory to access!\n");
        }
    #endif /* ACCESS_MEMORY */
    
    uint64_t threshold = calculate_threshold();
    
    int hits = 0;
    for(int i = 0; i < repeats; i++) {
        hits += prefetch((rand64() % 2048) + 512, accesses, aligned, flush_all) < threshold;
    }
    
    INFO("threshold: %zu\n", threshold);
    RESULT("setup_time: %zu\n", setup_end - setup_start);
    RESULT("hits: %d\n", hits);
    RESULT("prefetch_time: %zu\n", prefetch_time);
    RESULT("gadget_time: %zu\n", gadget_time);
    
    time_destroy();
    victim_destroy();
}
