OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

KERNEL_VMA    = 0xffffffff80000000;
KERNEL_LMA    = 0x00100000;
KERNEL_OFFSET = KERNEL_VMA - KERNEL_LMA;

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = KERNEL_VMA;

    .multiboot ALIGN(4K) :
        AT(ADDR(.multiboot) - KERNEL_OFFSET)
    {
        KEEP(*(.multiboot))
    } :text

    .text ALIGN(4K) :
        AT(ADDR(.text) - KERNEL_OFFSET)
    {
        *(.text*)
    } :text

    .rodata ALIGN(4K) :
        AT(ADDR(.rodata) - KERNEL_OFFSET)
    {
        *(.rodata*)
    } :text

    .eh_frame ALIGN(4K) :
        AT(ADDR(.eh_frame) - KERNEL_OFFSET)
    {
        *(.eh_frame*)
    } :text

    . = ALIGN(4K);

    .data ALIGN(4K) :
        AT(ADDR(.data) - KERNEL_OFFSET)
    {
        *(.data*)
        *(.sdata*)
    } :data

    .bss ALIGN(4K) (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)

        . = ALIGN(16);

        _kernel_stack_start = .;
        . += 0x8000;
        _kernel_stack_end = .;
        _kernel_stack_start_phys = _kernel_stack_start - KERNEL_OFFSET;
        _kernel_stack_end_phys   = _kernel_stack_end   - KERNEL_OFFSET;
    } :data

    _end = .;
}
