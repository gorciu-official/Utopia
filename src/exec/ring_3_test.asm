bits 64 

default rel

global ring_3_program_start
global ring_3_program_end
global ring_3_program 

ring_3_program_start:

ring_3_program:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg 
    mov rdx, msg_len
    syscall

    mov rax, 60
    syscall

msg db "works" 
msg_len equ $ - msg

ring_3_program_end:
