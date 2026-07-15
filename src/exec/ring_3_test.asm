bits 64 

default rel

global ring_3_program_start
global ring_3_program_end
global ring_3_program 

ring_3_program_start:

ring_3_program:
    mov rax, 1 
    mov rdi, 1 
    lea rsi, [rel msg2]
    mov rdx, msg2_len
    syscall

    xor rax, rax 
    xor rdi, rdi
    lea rsi, [rel buffer]
    mov rdx, 4
    syscall 

    mov rax, 1
    mov rdi, 1
    lea rsi, [rel buffer]
    mov rdx, 3
    syscall
    
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg]
    mov rdx, msg_len
    syscall

    mov rax, 60
    syscall

msg db 10, 0x1b, "[31mPOLSKA GUROM", 0x1b, "[0m", 10 
msg_len equ $ - msg

msg2 db 0x1b, "[32mwpisuj cos chlopie", 0x1b, "[0m", 10 
msg2_len equ $ - msg2

buffer:
    times 4 db 0 

ring_3_program_end:
