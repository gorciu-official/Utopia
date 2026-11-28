#include <constants.h>
#include <config.h>

#if (ARCHITECTURE == ARCHITECTURE_CODE_x86_64) && defined(CONFIG_ACPI_AML)

#include <types.h>
#include <mm/pmm.h>
#include <exec/scheduler.h>
#include <lib/string.h>
#include <lib/bug.h>

#include "opcodes.h"
#include "reader.h"

static aml_parser_t parser = {0};

static bool aml_skip_data_object(aml_parser_t* parser) {
    uint8_t op;

    if (!aml_read_byte(parser, &op))
        return false;

    switch (op) {
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
        return true;

    case AML_BYTE_PREFIX: {
        uint8_t v;
        return aml_read_byte(parser, &v);
    }

    case AML_WORD_PREFIX: {
        uint16_t v;
        return aml_read_u16(parser, &v);
    }

    case AML_DWORD_PREFIX: {
        uint32_t v;
        return aml_read_u32(parser, &v);
    }

    case AML_QWORD_PREFIX: {
        uint64_t v;
        return aml_read_u64(parser, &v);
    }

    case AML_STRING_PREFIX: {
        uint8_t c;

        do {
            if (!aml_read_byte(parser, &c))
                return false;
        } while (c != 0);

        return true;
    }

    case AML_BUFFER_OP:
    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP: {
        size_t end;

        if (!aml_read_pkg_length(parser, &end))
            return false;

        parser->position = end;

        return true;
    }

    default:
        return false;
    }
}

static const char* aml_indent(size_t depth) {
    static const char spaces[] = "                                  ";
    size_t n = depth * 2 + 2;

    if (n > sizeof(spaces) - 1)
        n = sizeof(spaces) - 1;

    return spaces + (sizeof(spaces) - 1 - n);
}

static bool aml_parse_term_list(
    aml_parser_t* parser,
    size_t end,
    size_t depth
) {
    char name[AML_NAME_MAX];

    if (depth > AML_MAX_DEPTH) {
        printk(KMT_ERROR, "ACPI", "AML nesting too deep");

        return false;
    }

    while (parser->position < end) {
        size_t op_pos = parser->position;
        uint8_t op;
        size_t pkg_end;

        if (!aml_read_byte(parser, &op))
            return false;

        switch (op) {
        case AML_SCOPE_OP:
            if (
                !aml_read_pkg_length(parser, &pkg_end) ||
                !aml_read_name_string(parser, name, sizeof(name))
            )
                goto malformed;

            printk(KMT_LOG, "ACPI", "%sScope(%s)", aml_indent(depth), name);

            if (
                pkg_end > end ||
                !aml_parse_term_list(parser, pkg_end, depth + 1)
            )
                return false;

            parser->position = pkg_end;

            break;

        case AML_NAME_OP:
            if (
                !aml_read_name_string(parser, name, sizeof(name)) ||
                !aml_skip_data_object(parser)
            )
                goto malformed;

            printk(KMT_LOG, "ACPI", "%sName(%s)", aml_indent(depth), name);

            break;

        case AML_METHOD_OP: {
            uint8_t flags;

            if (
                !aml_read_pkg_length(parser, &pkg_end) ||
                !aml_read_name_string(parser, name, sizeof(name)) ||
                !aml_read_byte(parser, &flags)
            )
                goto malformed;

            printk(
                KMT_LOG, "ACPI", "%sMethod(%s, args=%u)",
                aml_indent(depth), name, (unsigned)(flags & 7)
            );

            if (pkg_end > end)
                goto malformed;

            parser->position = pkg_end;

            break;
        }

        case AML_EXT_OP_PREFIX: {
            uint8_t ext;

            if (!aml_read_byte(parser, &ext))
                goto malformed;

            switch (ext) {
            case AML_EXT_DEVICE_OP:
            case AML_EXT_THERMALZONE_OP:
                if (
                    !aml_read_pkg_length(parser, &pkg_end) ||
                    !aml_read_name_string(parser, name, sizeof(name))
                )
                    goto malformed;

                printk(
                    KMT_LOG, "ACPI", "%s%s(%s)", aml_indent(depth),
                    ext == AML_EXT_DEVICE_OP ? "Device" : "ThermalZone",
                    name
                );

                if (
                    pkg_end > end ||
                    !aml_parse_term_list(parser, pkg_end, depth + 1)
                )
                    return false;

                parser->position = pkg_end;

                break;

            case AML_EXT_OPREGION_OP: {
                uint8_t space;

                if (
                    !aml_read_name_string(parser, name, sizeof(name)) ||
                    !aml_read_byte(parser, &space)
                )
                    goto malformed;

                if (
                    !aml_skip_data_object(parser) ||
                    !aml_skip_data_object(parser)
                )
                    goto malformed;

                printk(
                    KMT_LOG, "ACPI", "%sOperationRegion(%s, space=%u)",
                    aml_indent(depth), name, (unsigned)space
                );

                break;
            }

            case AML_EXT_FIELD_OP:
            case AML_EXT_PROCESSOR_OP:
            case AML_EXT_POWERRES_OP:
            case AML_EXT_MUTEX_OP:
                if (ext == AML_EXT_MUTEX_OP) {
                    uint8_t sync;

                    if (
                        !aml_read_name_string(parser, name, sizeof(name)) ||
                        !aml_read_byte(parser, &sync)
                    )
                        goto malformed;

                    break;
                }

                if (
                    !aml_read_pkg_length(parser, &pkg_end) ||
                    pkg_end > end
                )
                    goto malformed;

                parser->position = pkg_end;

                break;

            default:
                printk(
                    KMT_ERROR, "ACPI",
                    "Unhandled AML ext opcode 0x5B 0x%x at 0x%x",
                    (unsigned)ext, (unsigned)op_pos
                );

                return false;
            }

            break;
        }

        default:
            printk(
                KMT_ERROR, "ACPI",
                "Unhandled AML opcode 0x%x at 0x%x",
                (unsigned)op, (unsigned)op_pos
            );

            return false;
        }
    }

    return true;

malformed:
    printk(
        KMT_ERROR, "ACPI", "Malformed AML near %p", parser->position
    );

    return false;
}

static void acpi_aml_interpreter(void* arg) {
    (void)arg;

    printk(
        KMT_LOG, "ACPI", "AML: Parsing AML at %p (size: %u)",
        parser.base, parser.size
    );

    if (!aml_parse_term_list(&parser, parser.size, 0))
        printk(KMT_ERROR, "ACPI", "AML parse aborted.");
    else
        printk(KMT_LOG, "ACPI", "AML parse done.");

    thread_exit();
}

static thread_stack_t* acpi_aml_stack_alloc(void) {
    thread_stack_t* stack = malloc(sizeof(thread_stack_t));

    if (!stack)
        return NULL;

    memset(stack, 0, sizeof(thread_stack_t));

    stack->size = THREAD_KERNEL_STACK_SIZE;
    stack->base = malloc(stack->size);

    if (!stack->base) {
        free(stack);

        return NULL;
    }

    stack->ptr = (char*)stack->base + stack->size;

    return stack;
}

void acpi_aml_start_interpreter(char* base, size_t length) {
    parser.base = (const uint8_t*)base;
    parser.size = length;
    parser.position = 0;

    thread_stack_t* stack = acpi_aml_stack_alloc();

    if (!stack)
        return printk(
            KMT_ERROR, "ACPI",
            "Stack allocation for AML interpreter failed"
        );

    thread_t* thread = thread_create(
        "aml-interpreter", acpi_aml_interpreter, 0, 0, stack
    );

    if (!thread) {
        free(stack->base);
        free(stack);

        return printk(
            KMT_ERROR, "ACPI",
            "AML interpreter thread creation failed"
        );
    }

    scheduler_enqueue(thread);
}

#endif
