global syscall_entry

extern syscall_handler

syscall_entry:
    swapgs      
    
    push rcx
    push r11  

    call syscall_handler

    pop r11
    pop rcx

    swapgs
    sysretq
