bits 16

section .trampoline

global ap_start
ap_start:
    cli
    
    mov ax, cs
    mov ds, ax
    
    lgdt [cs:gdt_ptr_off]
    
    mov eax, cr0
    or al, 1
    mov cr0, eax  
    
    jmp short pm_entry
pm_entry:

    mov ax, 0x08   
    mov fs, ax   
    
    mov eax, cr0
    and al, 0xFE
    mov cr0, eax

    db 0xEA
    dw rm_entry_off
    dw 0x7000

rm_entry:
    mov ax, cs
    mov ds, ax
    
    mov eax, 1
    cpuid
    shr ebx, 24 
    
    mov si, ap_alive_table_ptr_off
    mov edi, dword [si]     
    
    mov byte [fs:edi + ebx], 1

halt_loop:
    hlt
    jmp halt_loop

align 8
gdt:
    dq 0 
    dq 0x00cf92000000ffff   
gdt_off equ (gdt - ap_start)

gdt_ptr:
    dw $ - gdt - 1
    dd 0x70000 + gdt_off 
gdt_ptr_off equ (gdt_ptr - ap_start)

global ap_alive_table_ptr
ap_alive_table_ptr: 
    dd 0  
ap_alive_table_ptr_off equ (ap_alive_table_ptr - ap_start)

rm_entry_off equ (rm_entry - ap_start)

global ap_end
ap_end:
