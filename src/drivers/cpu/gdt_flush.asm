[bits 64]
global gdt_flush

gdt_flush:
    lgdt [rdi]
    mov ax, 0x10 ; Kernel Data Segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    push 0x08
    lea rax, [rel .next]
    push rax
    retfq
.next:
    ret
