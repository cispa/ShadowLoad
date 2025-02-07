.global load_gadget_start
.global load_gadget_end

/* architecture dependent code */
#if defined(__x86_64__)
  .intel_syntax noprefix
  .text
  load_gadget_start:
  mov rax, [rdi]
  ret
  load_gadget_end:
  nop
#elif defined (__aarch64__)
  .text
  load_gadget_start:
  ldrb w0, [x0]
  ret
  load_gadget_end:
  nop
#else /* unknown architecture*/
 #error Unknown architecture. Only x86_64 and aarch64 are supported!
#endif /* architecture */
