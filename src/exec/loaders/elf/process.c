#include <types.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <arch/memory.h>
#include <exec/process.h>
#include <exec/loaders/elf.h>
#include <exec/scheduler.h>
#include <lib/screen.h>
#include <lib/string.h>

#include "elf_internal.h"

static void free_image_pages(uint64_t *l4_table) {
    vm_page_list_t pages = {0};
    if (vm_collect_user_pages(l4_table, 0, 0, &pages) == 0)
        vm_page_list_free(&pages);
}

static thread_stack_t *create_user_stack(uint64_t *l4_table) {
    const size_t size = USER_STACK_SIZE;
    const uint64_t low = USER_STACK_TOP - size;

    void *base = page_alloc(size / 0x1000);
    if (!base) return NULL;
    memset(base, 0, size);

    thread_stack_t *stack = (thread_stack_t *)malloc(sizeof(thread_stack_t));
    if (!stack) {
        free(base);
        return NULL;
    }
    stack->base = base;
    stack->size = size;
    stack->user_mapped = true;

    for (uint64_t offset = 0; offset < size; offset += 0x1000) {
        if (map_page_4k(l4_table, low + offset, hhdm_virt_to_phys((uint8_t *)base + offset),
                        VMF_USER | VMF_WRITE) != 0) {
            free(base);
            free(stack);
            return NULL;
        }
    }

    return stack;
}

void elf_prepared_destroy(elf_prepared_t *prepared) {
    if (!prepared) return;

    if (prepared->l4) free_page_table(prepared->l4);
    vm_page_list_free(&prepared->pages);

    if (prepared->stack) {
        if (prepared->stack->base) free(prepared->stack->base);
        free(prepared->stack);
    }

    memset(prepared, 0, sizeof(*prepared));
}

int elf_prepare(const create_elf_process_request_t *request, elf_prepared_t *out) {
    memset(out, 0, sizeof(*out));

    uint64_t *l4 = clone_page_table();
    if (!l4) return -1;

    elf_load_result_t main_result;
    elf_auxv_info_t auxv;
    uint64_t entry;

    int rc = elf_load_full(request->image.data, request->image.size, l4, &main_result, &auxv, &entry);
    if (rc != 0) {
        free_image_pages(l4);
        free_page_table(l4);
        return rc;
    }

    out->l4 = l4;
    out->entry = entry;
    out->brk_start = (main_result.highest_vaddr + 0xFFFULL) & ~0xFFFULL;

    if (vm_collect_user_pages(l4, 0, 0, &out->pages) != 0) goto fail;

    out->stack = create_user_stack(l4);
    if (!out->stack) {
        printk(KMT_LOG, "ELF Loader", "Failed to allocate stack for process thread!");
        goto fail;
    }

    if (elf_build_stack(request, &auxv, out->stack->base, out->stack->size, &out->sp) != 0)
        goto fail;

    out->stack->ptr = (void *)(uintptr_t)out->sp;
    return 0;

fail:
    elf_prepared_destroy(out);
    return -1;
}

int elf_start(create_elf_process_request_t request) {
    elf_prepared_t prepared;
    if (elf_prepare(&request, &prepared) != 0) return -1;

    process_t *process = process_create("jakis-elf", (void (*)(void *))prepared.entry,
                                        request.arg, 3, prepared.stack);
    if (!process) {
        elf_prepared_destroy(&prepared);
        return -1;
    }

    if (process->page_table) free_page_table(process->page_table);
    process->page_table = prepared.l4;
    process->owned_pages = prepared.pages;

    process->brk_start = prepared.brk_start;
    process->brk_current = process->brk_start;
    process->mmap_start = 0x400000000000;
    process->mmap_current = process->mmap_start;
    process->syscall_conv = request.syscall_conv;

    scheduler_enqueue(process->main_thread);
    return 0;
}
