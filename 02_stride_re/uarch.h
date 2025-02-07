#ifndef UARCH_H
#define UARCH_H

extern void _load_gadget_asm_start(void);
extern void _load_gadget_asm_end(void);

#ifdef __x86_64__
    #define PAGE_SIZE 4096
    #define CACHE_LINE_SIZE 64
    
    
    #define REG_ARG_1 "rdi"
    #define _maccess(pre, addr) asm volatile(pre "mov (%0), %%al" :: "r" (addr) : "rax")
    #define maccess(addr) _maccess("", addr)
    
    #define mfence() asm volatile("mfence")
    
    #define flush(addr) asm volatile("clflush (%0)" :: "r" (addr))
    
    #define return_asm() "ret"
    
    #define nop() asm volatile("nop")
    
    #define VIRTUAL_ADDRESS_BITS 48
#elif defined (__aarch64__)
    #define PAGE_SIZE 4096
    #define CACHE_LINE_SIZE 64
    
    
    #define REG_ARG_1 "x0"
    #define _maccess(pre, addr) asm volatile(pre "ldrb w0, [%0]" :: "r" (addr) : "x0")
    #define maccess(addr) _maccess("", addr)
    
    #define mfence() asm volatile("DMB SY\nISB")
    
    #define flush(addr) asm volatile("DC CIVAC, %0" :: "r" (addr))
    
    #define return_asm() "ret"
    
    #define nop() asm volatile("nop")
    
    #define VIRTUAL_ADDRESS_BITS 48
#else
    #error "unknown architecture. Only x86_64 and aarch64 are supported"
#endif

#endif /* UARCH_H */
