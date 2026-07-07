OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)
PHDRS
{
    text PT_LOAD FLAGS(5);  /* RX */
    data PT_LOAD FLAGS(6);  /* RW */
}
SECTIONS
{
    . = 0x100000;
    .multiboot : { *(.multiboot) } :text
    .text : { *(.text) } :text
    .data : { *(.data) } :data
    .bss  : { *(.bss)  } :data
    .trampoline  : { *(.trampoline)  } :data
    .stack :
    {
        . = ALIGN(16);
        _kernel_stack_start = .;
        . = . + 0x8000;
        _kernel_stack_end = .;
    } :data

    _end = .;
}
