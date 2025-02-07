// make sure the addresses are accessible from C code
.global load_gadget_start
.global load_gadget_end

.intel_syntax noprefix

.text
// void load_gadget_start(uint8_t* addr){*addr;}
load_gadget_start:
    mov al, [rdi]
    ret
load_gadget_end:
    nop
