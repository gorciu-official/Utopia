global _start
global _long_mode_entry
extern kinit

%define KERNEL_VMA     0xffffffff80000000
%define KERNEL_LMA     0x00100000
%define KERNEL_OFFSET  0xffffffff7ff00000

%define HHDM_BASE      0xffff800000000000
%define PHYS(addr) ((addr) - KERNEL_OFFSET)

; multiboot header type shi-

section .multiboot
bits 32

align 4

header_start:
    dd 0x1BADB002
    dd 0x00000007
    dd -(0x1BADB002 + 0x00000007)

    dd 0
    dd 0
    dd 0
    dd 0
    dd 0

    dd 0
    dd 800
    dd 600
    dd 32

header_end:

section .text
bits 32

_start:
    cli

    ; temporary stack
    mov esp, PHYS(stack_top)

    ; ptr to multiboot_info_t on stack
    push ebx
    
    ; 64-bit type shi-
    call check_cpuid
    call check_long_mode
    call setup_page_tables
    call enable_paging

    ; loading gdt64 
    mov eax, PHYS(gdt64_pointer)
    lgdt [eax]

    ; enter long mode (using identity mapping below 4GB)
    jmp gdt64.code_segment:PHYS(long_mode_trampoline)

    cli
    hlt
    jmp $

section .text
bits 64

long_mode_trampoline:
    ; magic to jump to a virtual address because it fucks up otherwise
    mov rax, long_mode_high_entry
    jmp rax

long_mode_high_entry:
    ; recovering that stack-saved multiboot slop 
    mov eax, dword [PHYS(stack_top) - 4]

    test eax, eax
    jz .no_multiboot
    
    ; converting multiboot shit to virtual (yes)
    mov edi, eax
    mov rax, HHDM_BASE
    add rdi, rax
    
    ; after all stack shit operations are done, we are again
    ; switching stacks bc old one was identity mapping etc we don't 
    ; want you brother
    mov rsp, stack_top
    and rsp, -16


    ; nuke identity mapping 
    mov rax, PHYS(page_table_l4)

    mov qword [rax + 0*8], 0
    mov qword [rax + 1*8], 0
    mov qword [rax + 2*8], 0
    mov qword [rax + 3*8], 0

    ; Mr. President, we are launching nuclear 
    ; bombs at identity mapping in your kernel
    mov cr3, rax

    ; enterprise, troje do teleportacji
    call kinit

.hang:
    cli
    hlt
    jmp .hang


.no_multiboot:
    ; shit multiboot fucked up
    mov al, 'M'
    jmp error

bits 32

check_cpuid:
    pushfd
    pop eax

    mov ecx, eax

    xor eax, 1 << 21

    push eax
    popfd

    pushfd
    pop eax

    push ecx
    popfd

    cmp eax, ecx
    je .no_cpuid

    ret

.no_cpuid:
    mov al, 'C'
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid

    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid

    test edx, 1 << 29
    jz .no_long_mode

    ret

.no_long_mode:
    mov al, 'L'
    jmp error

setup_page_tables:
    ; --------------------------------------------------------
    ; ESI = physical PML4
    ; --------------------------------------------------------

    mov esi, PHYS(page_table_l4)


    ; ========================================================
    ; IDENTITY MAP 0..4GB
    ; ========================================================

    ; --------------------------------------------------------
    ; PML4[0] -> L3
    ; --------------------------------------------------------

    mov eax, PHYS(page_table_l3)
    or eax, 0b11

    mov [esi + 0*8], eax
    mov dword [esi + 0*8 + 4], 0


    ; --------------------------------------------------------
    ; L3[0] -> first L2
    ; --------------------------------------------------------

    mov edi, PHYS(page_table_l3)

    mov eax, PHYS(page_table_l2_first)
    or eax, 0b11

    mov [edi + 0*8], eax
    mov dword [edi + 0*8 + 4], 0


    ; --------------------------------------------------------
    ; L3[1] -> L2 page 0
    ; --------------------------------------------------------

    mov eax, PHYS(page_table_l2)
    or eax, 0b11

    mov [edi + 1*8], eax
    mov dword [edi + 1*8 + 4], 0


    ; --------------------------------------------------------
    ; L3[2] -> L2 page 1
    ; --------------------------------------------------------

    mov eax, PHYS(page_table_l2) + 4096
    or eax, 0b11

    mov [edi + 2*8], eax
    mov dword [edi + 2*8 + 4], 0


    ; --------------------------------------------------------
    ; L3[3] -> L2 page 2
    ; --------------------------------------------------------

    mov eax, PHYS(page_table_l2) + 8192
    or eax, 0b11

    mov [edi + 3*8], eax
    mov dword [edi + 3*8 + 4], 0


    ; ========================================================
    ; FIRST 2MB USING 4KB PAGES
    ; ========================================================

    mov eax, PHYS(page_table_l1_low)
    or eax, 0b11

    mov edi, PHYS(page_table_l2_first)

    mov [edi + 0*8], eax
    mov dword [edi + 0*8 + 4], 0


    xor ecx, ecx

.loop_l1:

    mov eax, ecx
    shl eax, 12

    or eax, 0b11

    mov edi, PHYS(page_table_l1_low)

    mov [edi + ecx*8], eax
    mov dword [edi + ecx*8 + 4], 0

    inc ecx

    cmp ecx, 512
    jne .loop_l1


    ; ========================================================
    ; 2MB .. 1GB
    ; ========================================================

    mov ecx, 1

.loop_l2_first:

    mov eax, ecx
    shl eax, 21

    or eax, 0b10000011

    mov edi, PHYS(page_table_l2_first)

    mov [edi + ecx*8], eax
    mov dword [edi + ecx*8 + 4], 0

    inc ecx

    cmp ecx, 512
    jne .loop_l2_first


    ; ========================================================
    ; 1GB .. 4GB
    ; ========================================================

    xor ecx, ecx

.loop_l2:
    mov eax, ecx

    add eax, 512
    shl eax, 21

    or eax, 0b10000011

    mov edi, PHYS(page_table_l2)

    mov [edi + ecx*8], eax
    mov dword [edi + ecx*8 + 4], 0

    inc ecx

    cmp ecx, 1536
    jne .loop_l2

    mov eax, PHYS(page_table_l3_kernel)
    or eax, 0b11

    mov [esi + 511*8], eax
    mov dword [esi + 511*8 + 4], 0

    mov edi, PHYS(page_table_l3_kernel)

    mov eax, PHYS(page_table_l2_kernel)
    or eax, 0b11

    mov [edi + 510*8], eax
    mov dword [edi + 510*8 + 4], 0

    mov edi, PHYS(page_table_l2_kernel)

    mov eax, PHYS(page_table_l1_kernel)
    or eax, 0b11

    mov [edi + 0*8], eax
    mov dword [edi + 0*8 + 4], 0

    xor ecx, ecx

    mov edi, 0x00100000

.loop_kernel_l1:
    mov eax, edi
    or eax, 0b11

    mov ebx, PHYS(page_table_l1_kernel)

    mov [ebx + ecx*8], eax
    mov dword [ebx + ecx*8 + 4], 0

    add edi, 0x1000

    inc ecx

    cmp ecx, 512
    jne .loop_kernel_l1

    mov ecx, 1

    mov edi, 0x00200000

.loop_kernel_l2:
    mov eax, edi

    or eax, 0b10000011

    mov ebx, PHYS(page_table_l2_kernel)

    mov [ebx + ecx*8], eax
    mov dword [ebx + ecx*8 + 4], 0

    add edi, 0x200000

    inc ecx

    cmp ecx, 512
    jne .loop_kernel_l2

    mov eax, PHYS(page_table_l3_hhdm)
    or eax, 0b11

    mov [esi + 256*8], eax
    mov dword [esi + 256*8 + 4], 0

    mov edi, PHYS(page_table_l3_hhdm)

    mov eax, PHYS(page_table_l2_hhdm)
    or eax, 0b11

    mov [edi + 0*8], eax
    mov dword [edi + 0*8 + 4], 0


    mov eax, PHYS(page_table_l2_hhdm) + 4096
    or eax, 0b11

    mov [edi + 1*8], eax
    mov dword [edi + 1*8 + 4], 0


    mov eax, PHYS(page_table_l2_hhdm) + 8192
    or eax, 0b11

    mov [edi + 2*8], eax
    mov dword [edi + 2*8 + 4], 0


    mov eax, PHYS(page_table_l2_hhdm) + 12288
    or eax, 0b11

    mov [edi + 3*8], eax
    mov dword [edi + 3*8 + 4], 0

    xor ecx, ecx

.hhdm_fill:
    ; ecx = global 2MB page index
    ;
    ; table = ecx / 512
    ; index = ecx % 512

    mov eax, ecx
    shr eax, 9

    ; eax = table number 0..3

    mov edx, ecx
    and edx, 511

    ; edx = entry number 0..511

    mov eax, ecx
    shl eax, 21

    or eax, 0b10000011

    ; table = ecx >> 9

    mov ebx, ecx
    shr ebx, 9
    shl ebx, 12

    add ebx, PHYS(page_table_l2_hhdm)

    mov edx, ecx
    and edx, 511

    mov [ebx + edx*8], eax
    mov dword [ebx + edx*8 + 4], 0

    inc ecx

    cmp ecx, 2048
    jne .hhdm_fill
    ret

enable_paging:
    mov eax, PHYS(page_table_l4)
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

error:
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov dword [0xb8008], 0x4f204f20

    mov byte [0xb800a], al

    cli
    hlt

    jmp $

section .bss

align 4096

global page_table_l4
page_table_l4:
    resb 4096

page_table_l3:
    resb 4096

page_table_l2_first:
    resb 4096

page_table_l1_low:
    resb 4096

page_table_l1_kernel:
    resb 4096

page_table_l2:
    resb 4096 * 3

page_table_l3_kernel:
    resb 4096

page_table_l2_kernel:
    resb 4096

page_table_l3_hhdm:
    resb 4096

page_table_l2_hhdm:
    resb 4096 * 4

align 16

stack_bottom:
    resb 4096 * 4

stack_top:

section .rodata

align 8

global gdt64

gdt64:
    dq 0

.code_segment equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)

gdt64_end:

global gdt64_pointer

gdt64_pointer:
    dw gdt64_end - gdt64 - 1
    dd PHYS(gdt64)
