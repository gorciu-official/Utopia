global syscall_entry

extern syscall_handler

syscall_entry:
    swapgs               
    mov [gs:0x10], rsp     
    mov rsp, [gs:0x00]   
    
    push qword [gs:0x10]  
    push r11   
    push rcx

    call syscall_handler

    pop rcx 
    pop r11    
    pop rsp 
    
    swapgs       
    o64 sysret        
