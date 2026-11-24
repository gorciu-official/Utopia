#include <types.h>
#include <mm/pmm.h>
#include <arch/memory.h>
#include <lib/string.h>

#include "elf_internal.h"

#define ELF_DEFAULT_ARGV0  "/init"
#define ELF_RANDOM_SIZE    16
#define ELF_AUX_PAIR_COUNT 14

typedef struct {
    uint8_t *base;
    uint64_t top;
    uint64_t sp;
    uint64_t user_top;
} elf_stack_builder_t;

typedef struct {
    uint64_t type;
    uint64_t value;
} elf_aux_pair_t;

static uint64_t stack_user_address(const elf_stack_builder_t *builder, uint64_t kernel_address) {
    return builder->user_top - (builder->top - kernel_address);
}

static bool stack_reserve(elf_stack_builder_t *builder, uint64_t length) {
    uint64_t base = (uint64_t)builder->base;
    if (builder->sp < base || length > builder->sp - base) return false;
    builder->sp -= length;
    return true;
}

static bool stack_push_bytes(elf_stack_builder_t *builder, const void *data, uint64_t length,
                             uint64_t *out_user_address) {
    if (!stack_reserve(builder, length)) return false;
    if (length) memcpy((void *)(uintptr_t)builder->sp, data, length);
    if (out_user_address) *out_user_address = stack_user_address(builder, builder->sp);
    return true;
}

static bool stack_push_string(elf_stack_builder_t *builder, const char *string, uint64_t *out_user_address) {
    return stack_push_bytes(builder, string, (uint64_t)strlen(string) + 1, out_user_address);
}

static bool stack_push_strings(elf_stack_builder_t *builder, char *const *strings, int count,
                               uint64_t *addresses) {
    for (int i = count - 1; i >= 0; i--) {
        if (!stack_push_string(builder, strings[i], &addresses[i])) return false;
    }
    return true;
}

static bool stack_push_random(elf_stack_builder_t *builder, uint64_t *out_user_address) {
    builder->sp &= ~0xFULL;
    if (!stack_reserve(builder, ELF_RANDOM_SIZE)) return false;

    uint8_t *bytes = (uint8_t *)(uintptr_t)builder->sp;
    for (uint64_t i = 0; i < ELF_RANDOM_SIZE; i++)
        bytes[i] = (uint8_t)(i ^ 0x42); // TODO: real entropy

    *out_user_address = stack_user_address(builder, builder->sp);
    return true;
}

static bool stack_push_words(elf_stack_builder_t *builder, const uint64_t *words, uint64_t count) {
    uint64_t base = (uint64_t)builder->base;
    if (count > ~0ULL / sizeof(uint64_t)) return false;

    uint64_t size = count * sizeof(uint64_t);
    if (builder->sp < base || size > builder->sp - base) return false;

    builder->sp = (builder->sp - size) & ~0xFULL;
    if (builder->sp < base) return false;

    memcpy((void *)(uintptr_t)builder->sp, words, size);
    return true;
}

static size_t build_auxv(const elf_auxv_info_t *auxv, uint64_t random_address,
                         uint64_t execfn_address, elf_aux_pair_t *pairs, size_t capacity) {
    size_t count = 0;

    #define PUSH_AUX(type, value) do { \
        if (count >= capacity) return 0; \
        pairs[count++] = (elf_aux_pair_t){ (type), (value) }; \
    } while (0)

    PUSH_AUX(AT_PHDR, auxv->at_phdr);
    PUSH_AUX(AT_PHENT, auxv->at_phent);
    PUSH_AUX(AT_PHNUM, auxv->at_phnum);
    PUSH_AUX(AT_PAGESZ, 4096);
    PUSH_AUX(AT_BASE, auxv->has_interp ? auxv->at_base : 0);
    PUSH_AUX(AT_ENTRY, auxv->at_entry);
    PUSH_AUX(AT_UID, 0);
    PUSH_AUX(AT_EUID, 0);
    PUSH_AUX(AT_GID, 0);
    PUSH_AUX(AT_EGID, 0);
    PUSH_AUX(AT_SECURE, 0);
    PUSH_AUX(AT_CLKTCK, 100);
    PUSH_AUX(AT_RANDOM, random_address);
    PUSH_AUX(AT_EXECFN, execfn_address);

    #undef PUSH_AUX

    return count;
}

int elf_build_stack(const create_elf_process_request_t *request, const elf_auxv_info_t *auxv,
                    void *stack_base, size_t stack_size, uint64_t *out_sp) {
    elf_stack_builder_t builder = {
        .base = (uint8_t *)stack_base,
        .top = (uint64_t)stack_base + stack_size,
        .sp = (uint64_t)stack_base + stack_size,
        .user_top = USER_STACK_TOP,
    };

    char *default_argv[] = { (char *)ELF_DEFAULT_ARGV0 };
    int argc = request->args.argc;
    char **argv = request->args.argv;
    if (argc < 1 || !argv) {
        argc = 1;
        argv = default_argv;
    }

    int envc = 0;
    if (request->args.envp) {
        while (request->args.envp[envc]) envc++;
    }

    uint64_t *argv_addresses = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)argc);
    uint64_t *envp_addresses = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)(envc ? envc : 1));
    uint64_t *words = NULL;
    bool ok = argv_addresses && envp_addresses;

    if (ok) ok = stack_push_strings(&builder, request->args.envp, envc, envp_addresses);
    if (ok) ok = stack_push_strings(&builder, argv, argc, argv_addresses);

    uint64_t random_address = 0;
    if (ok) ok = stack_push_random(&builder, &random_address);

    elf_aux_pair_t auxv_pairs[ELF_AUX_PAIR_COUNT];
    uint64_t execfn_address = argv_addresses ? argv_addresses[0] : 0;
    size_t auxv_count = build_auxv(auxv, random_address, execfn_address,
                                   auxv_pairs, ELF_AUX_PAIR_COUNT);
    if (auxv_count == 0) ok = false;

    uint64_t word_count = 1 + (uint64_t)argc + 1 + (uint64_t)envc + 1 + 2 * (uint64_t)auxv_count + 2;
    if (ok) {
        words = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)word_count);
        if (!words) ok = false;
    }

    if (ok) {
        size_t index = 0;

        words[index++] = (uint64_t)argc;
        for (int i = 0; i < argc; i++) words[index++] = argv_addresses[i];
        words[index++] = 0;

        for (int i = 0; i < envc; i++) words[index++] = envp_addresses[i];
        words[index++] = 0;

        for (size_t i = 0; i < auxv_count; i++) {
            words[index++] = auxv_pairs[i].type;
            words[index++] = auxv_pairs[i].value;
        }
        words[index++] = AT_NULL;
        words[index++] = 0;

        ok = stack_push_words(&builder, words, word_count);
    }

    if (ok) *out_sp = stack_user_address(&builder, builder.sp);

    free(words);
    free(envp_addresses);
    free(argv_addresses);
    return ok ? 0 : -1;
}
