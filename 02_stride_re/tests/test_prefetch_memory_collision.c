#include "tests/common.h"

#define MAX(a, b) (a > b ? a : b)

#ifdef ACCESS_MEMORY
    #define DUMMY_BUFFER_SIZE (PAGE_SIZE * 10)
    static uint8_t* dummy_buffer;
#endif /* ACCESS_MEMORY */

static uint8_t* colliding_buffer;

static uint64_t prefetch(int64_t stride, int accesses, uint64_t start_offset, uint64_t access_offset, uint64_t measure_offset) {
    
    victim_flush_buffer();

    mfence();
    
    #ifdef ACCESS_MEMORY
        // some CPUs seem to only prefetch if there is a lot of memory accesses recently
        for(uint64_t i = 0; i < DUMMY_BUFFER_SIZE; i += 64) maccess(&dummy_buffer[i]); 
    #endif /* ACCESS_MEMORY */
    
    #ifdef USE_NOP
        // this is required on some CPUs. Not 100% sure why, but without nopping, there is no prefetching sometimes.
        for(int i = 0; i < NOP_COUNT; i++) nop();
    #endif /* USE_NOP */
    
    
    // repeating 5 times is not necessary, but there is no reason not to (and it may increase chance of success)
    for(int repeat = 0; repeat < 5; repeat ++) {
        
        for(int access = 0; access < accesses; access++) {
            _victim_gadget(colliding_buffer + start_offset + (access % accesses) * stride); // hacky but should allow re-using victim gadget but to access non-victim buffer
            #ifdef USE_FENCE
                mfence();
            #endif /* USE_FENCE */
        }
        
        victim_load_gadget(access_offset);
        #ifdef USE_FENCE
            mfence();
        #endif /* USE_FENCE */
    }
    
    return victim_probe(measure_offset);
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
    
    if(argc != 9) {
        FATAL("usage %s <stride> <accesses> <start_offset> <access_offset> <measure_offset> <colliding_buffer_and> <colliding_buffer_xor> <repeats>\n", argv[0]);
    }
    
    if(time_init()) {
        FATAL("failed to initialize timer!\n");
    }

    if(victim_init()) {
        FATAL("failed to initialize victim!\n");
    }
    
    int64_t stride = strtoll(argv[1], NULL, 0);
    int accesses = atoi(argv[2]);
    uint64_t start_offset = strtoull(argv[3], NULL, 0);
    uint64_t access_offset = strtoull(argv[4], NULL, 0);
    uint64_t measure_offset = strtoull(argv[5], NULL, 0);
    uintptr_t colliding_buffer_address = (victim_buffer_address() & strtoull(argv[6], NULL, 0)) ^ strtoull(argv[7], NULL, 0);
    int repeats = atoi(argv[8]);
    
    DEBUG("arguments: stride=%zu, accesses=%d, start_offset=%zu, measure_offset=%zu colliding_buffer_and=0x%016zx colliding_buffer_xor=0x%016zx\n", stride, accesses, start_offset, measure_offset, strtoull(argv[6], NULL, 0), strtoull(argv[7], NULL, 0));
    
    int64_t required = MAX(MAX(measure_offset + 8, access_offset + 8), start_offset + stride * (accesses - 1) + 8);
    
    if(required > VICTIM_BUFFER_SIZE) {
        FATAL(
            "not enough space in victim buffer (required: %zu, available: %zu)!\n",
            required,
            (uint64_t) VICTIM_BUFFER_SIZE
        );
    }
    
    colliding_buffer = (uint8_t*)(colliding_buffer_address);
    
    for(uint64_t offset = 0; offset < VICTIM_BUFFER_SIZE + 4 * PAGE_SIZE; offset += PAGE_SIZE) {
        mmap((void*)(colliding_buffer_address + offset - (colliding_buffer_address % PAGE_SIZE)) - PAGE_SIZE * 2, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED_NOREPLACE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    }
    
    #ifdef ACCESS_MEMORY
        dummy_buffer = mmap(NULL, DUMMY_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
        if(dummy_buffer == MAP_FAILED) {
            FATAL("failed to map memory to access!\n");
        }
    #endif /* ACCESS_MEMORY */
    
    uint64_t threshold = calculate_threshold();
    
    INFO("threshold: %zu\n", threshold);
    
    int hits = 0;
    for(int repeat = 0; repeat < repeats; repeat ++) {
        hits += prefetch(stride, accesses, start_offset, access_offset, measure_offset) < threshold;
    }
    RESULT("%d\n", hits);
    
    // munmap(colliding_buffer, VICTIM_BUFFER_SIZE);
    
    time_destroy();
    victim_destroy();
}
