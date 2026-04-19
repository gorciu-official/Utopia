OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .multiboot : { *(.multiboot) }
    .text : { *(.text) }
    .data : { *(.data) }
    .bss  : { *(.bss)  }
    .stack :
    {
        . = ALIGN(16);
        _kernel_stack_start = .;
        . = . + 0x8000;
        _kernel_stack_end = .;
    }
}
