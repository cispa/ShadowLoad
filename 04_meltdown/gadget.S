.intel_syntax noprefix

.global load_gadget_start
.global load_gadget_end

load_gadget_start:
mov rax, [rdi]
ret
load_gadget_end:
nop
