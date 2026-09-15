#include <boot/limine.h>

// architecture specific limine settings 

__attribute__((used))
const volatile struct limine_paging_mode_request paging_mode = {
    .max_mode = 39, .min_mode = 39
};
