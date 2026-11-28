#include <types.h>
#include <mm/pmm.h>
#include <exec/scheduler.h>
#include <lib/string.h>
#include <lib/bug.h>

typedef struct {
    size_t size;
    size_t position;
    char* base;
} aml_parser_t;

static aml_parser_t parser = {0};

static void acpi_aml_interpreter(void* arg) {
    (void)arg;
    printk(KMT_LOG, "ACPI", "Hello world!");
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
    parser.base = base;
    parser.size = length;
    parser.position = 0;

    thread_stack_t* stack = acpi_aml_stack_alloc();
    if (!stack)
        return printk(KMT_ERROR, "ACPI", "Stack allocation for AML interpreter failed");

    thread_t* thread = thread_create("aml-interpreter", acpi_aml_interpreter, 0, 0, stack);
    if (!thread) {
        free(stack->base);
        free(stack);
        return printk(KMT_ERROR, "ACPI", "AML interpreter thread creation failed");
    }

    scheduler_enqueue(thread);
}
