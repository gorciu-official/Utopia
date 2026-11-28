#include <types.h>
#include <arch/x86_64/common.h>
#include <lib/screen.h>

#include "idt_structs.h"

static idt_entry_t idt_entries[256];
static idt_ptr_t idt_ptr;

extern uint64_t isr_stub_table[];
extern uint64_t irq_stub_table[];
extern void     isr0x40(void);
extern void     isr0x41(void);

extern void idt_load(uint64_t);
extern void isr_spurious(void);

#define IDT_IST_DOUBLE_FAULT 1
#define IDT_IST_NMI          2
#define IDT_SPURIOUS_VECTOR  0xFF

static void idt_set_gate_ist(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
    idt_entries[num].offset_low = (uint16_t)(base & 0xFFFF);
    idt_entries[num].selector = sel;
    idt_entries[num].ist = ist & 0x7;
    idt_entries[num].type_attr = flags;
    idt_entries[num].offset_mid = (uint16_t)((base >> 16) & 0xFFFF);
    idt_entries[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt_entries[num].reserved = 0;
}

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt_set_gate_ist(num, base, sel, flags, 0);
}

static volatile int idt_built = 0;

void idt_init() {
    if (!idt_built) {
        idt_ptr.limit = (sizeof(idt_entry_t) * 256) - 1;
        idt_ptr.base = (uint64_t)&idt_entries;

        for (int i = 0; i < 256; ++i) {
            idt_set_gate(i, 0, 0, 0);
        }

        for (int i = 0; i < 32; ++i) {
            uint8_t ist = 0;
            if (i == 8) ist = IDT_IST_DOUBLE_FAULT;
            if (i == 2) ist = IDT_IST_NMI;
            idt_set_gate_ist(i, isr_stub_table[i], 0x08, 0x8E, ist);
        }

        for (int i = 0; i < 16; ++i) {
            idt_set_gate(32 + i, irq_stub_table[i], 0x08, 0x8E);
        }

        idt_set_gate(0x40, (uintptr_t)isr0x40, 0x08, 0x8E);
        idt_set_gate(0x41, (uintptr_t)isr0x41, 0x08, 0x8E); // DPL0: kernel only

        idt_set_gate(IDT_SPURIOUS_VECTOR, (uintptr_t)isr_spurious, 0x08, 0x8E);

        __atomic_store_n(&idt_built, 1, __ATOMIC_RELEASE);
    }

    idt_load((uint64_t)&idt_ptr);

    asm volatile("sti");
}
