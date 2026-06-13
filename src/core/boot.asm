global _start
extern kmain 

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
bits 64
_long_mode_entry:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, [multiboot_ptr]
    call kmain 

    hlt

section .text
bits 32

_start:
    mov esp, stack_top

    mov [multiboot_ptr], ebx

	call check_cpuid
	call check_long_mode

	call setup_page_tables
	call enable_paging

	lgdt [gdt64_pointer]
	jmp gdt64.code_segment:_long_mode_entry

	hlt

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
	mov al, "C"
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
	mov al, "L"
	jmp error

setup_page_tables:
    mov eax, page_table_l3
    or eax, 0b11 
    mov [page_table_l4], eax

    mov ecx, 0
.loop_l3:
    mov eax, page_table_l2
    mov edx, 4096
    imul edx, ecx
    add eax, edx
    or eax, 0b11
    mov [page_table_l3 + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .loop_l3

    mov eax, page_table_l2_apic
    or eax, 0b11 
    mov [page_table_l3 + 3 * 8], eax 

    mov ecx, 0
.loop:
    mov eax, 0x200000 
    mov edx, ecx
    imul edx, eax
    or edx, 0b10000011    
    mov [page_table_l2 + ecx * 8], edx

    inc ecx
    cmp ecx, 2048
    jne .loop 

    mov ecx, 0
.loop_apic:
    mov eax, 0x200000
    mul ecx
    add eax, 0xC0000000   
    
    or eax, 0b10010011  
    
    mov [page_table_l2_apic + ecx * 8], eax

    inc ecx
    cmp ecx, 512
    jne .loop_apic

    ret

enable_paging:
	mov eax, page_table_l4
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
	mov byte  [0xb800a], al
	hlt

section .bss
align 4096
global page_table_l4
page_table_l4:
    resb 4096
page_table_l3:
    resb 4096
page_table_l2:
    resb 4096 * 4
page_table_l2_apic:
    resb 4096
stack_bottom:
    resb 4096 * 4
align 16
stack_top:

section .bss
global multiboot_ptr
multiboot_ptr:
    resq 1

section .rodata
global gdt64
gdt64:
	dq 0
.code_segment: equ $ - gdt64
	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
global gdt64_pointer
gdt64_pointer:
.pointer:
	dw $ - gdt64 - 1
	dq gdt64
