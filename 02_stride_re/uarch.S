
.global _load_gadget_asm_start
.global _load_gadget_asm_end

#ifdef __x86_64__
    .intel_syntax noprefix
    
    _load_gadget_asm_start:
        mov al, [rdi]
        ret
    _load_gadget_asm_end:
        nop
    
#elif defined (__aarch64__)
    _load_gadget_asm_start:
        ldrb w0, [x0]
        ret
    _load_gadget_asm_end:
        nop
#else
    #error "unknown architecture. Only x86_64 and aarch64 are supported."
#endif /* ARCHITECTURE */
