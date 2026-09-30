bits 16

section .trampoline

global ap_start
ap_start:
    cli
    
    lgdt [cs:gdt32_ptr - ap_start]
    
    mov eax, cr0
    or al, 1
    mov cr0, eax
    
    db 0x66, 0xEA
    dd (0x70000 + (pm_entry - ap_start))
    dw 0x08

bits 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    mov esp, 0x72000
    
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    mov eax, [0x70F00]
    mov cr3, eax
    
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr
    
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    push 0x18
    push (0x70000 + (long_mode_entry - ap_start))
    retf

bits 64
long_mode_entry:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov rax, 0x70F18
    mov rsp, [rax]
    
    mov rax, [0x70F08]
    lgdt [rax]
    
    mov rax, [0x70F10]
    jmp rax

.halt:
    hlt
    jmp .halt

align 8
gdt32:
    dq 0
    dq 0x00cf9a000000ffff
    dq 0x00cf92000000ffff 
    dq 0x00209a0000000000
gdt32_ptr:
    dw $ - gdt32 - 1
    dd 0x70000 + (gdt32 - ap_start)

align 16
times 0x0F00 - ($ - ap_start) db 0

global ap_shared_data
ap_shared_data:
    .cr3:       dd 0         ; 0x70F00
    .padding:   dd 0         ; 0x70F04
    .gdt_ptr:   dq 0         ; 0x70F08
    .main_ptr:  dq 0         ; 0x70F10
    .stack_ptr: dq 0         ; 0x70F18

global ap_end
ap_end:
