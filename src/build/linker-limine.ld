OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)
PHDRS
{
    text PT_LOAD FLAGS(5);  /* RX */
    data PT_LOAD FLAGS(6);  /* RW */
    limine_requests PT_LOAD;
}
SECTIONS
{
    /* this is a fix for "lower half PHDRs are not allowed" */
    . = 0xffffffff80000000;

    /* 
     * .multiboot : { *(.multiboot) } :text 
     * the simplest solution is to comment this so that it is not included in the kernel
    */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests 

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : { *(.text) } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        *(.rodata .rodata.*)
    } :data

    .note.gnu.build-id : {
        *(.note.gnu.build-id)
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : { *(.data) } :data
    .bss  : { *(.bss)  } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .trampoline  : { *(.trampoline)  } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .stack :
    {
        . = ALIGN(16);
        _kernel_stack_start = .;
        . = . + 0x8000;
        _kernel_stack_end = .;
    } :data
    
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }

    _end = .;
}
