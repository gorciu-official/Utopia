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
    
    mov esp, 0x70000
    
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    mov eax, [0x70000 + (ap_cr3_ptr - ap_start)]
    mov cr3, eax
    
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    mov eax, [0x70000 + (ap_gdt_ptr - ap_start)]
    lgdt [eax]
    
    push 0x08
    push (0x70000 + (long_mode_entry - ap_start))
    retf

bits 64
long_mode_entry:
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov rax, 0x70000 + (ap_stack_ptr_trampoline - ap_start)
    mov rsp, [rax]
    
    mov rax, 0x70000 + (ap_main_ptr - ap_start)
    mov rax, [rax]
    call rax
    
.halt:
    hlt
    jmp .halt

align 8
gdt32:
    dq 0
    dq 0x00cf9a000000ffff
    dq 0x00cf92000000ffff
gdt32_ptr:
    dw $ - gdt32 - 1
    dd 0x70000 + (gdt32 - ap_start)

global ap_cr3_ptr
ap_cr3_ptr: dd 0

global ap_gdt_ptr
ap_gdt_ptr: dd 0

global ap_main_ptr
ap_main_ptr: dq 0

global ap_stack_ptr_trampoline
ap_stack_ptr_trampoline: dq 0

global ap_end
ap_end:
