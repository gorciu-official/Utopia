bits 64 

default rel

section .text 

global ring_3_program_start
global ring_3_program_end
global ring_3_program 

ring_3_program_start:

ring_3_program:
    syscall
    ; for simplicity i wont add another interrupt to idt just to test whether the program works 
    ; if it doesnt work it should throw a page fault or a general protection fault anyways
.loop
    jmp .loop 

ring_3_program_end:
