global syscall_entry

extern syscall_handler

syscall_entry:
    swapgs               
    mov [gs:0x10], rsp     
    mov rsp, [gs:0x00]   
    
    push qword 0x23 
    push qword [gs:0x10] 
    push r11 
    push qword 0x1b  
    push rcx  
    push qword 0  
    push qword 0 

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp      
    sub rsp, 8
    call syscall_handler
    add rsp, 8

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    add rsp, 16    
    pop rcx           
    add rsp, 8        
    pop r11
    pop rsp   

    swapgs       
    o64 sysret        

