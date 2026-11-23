#pragma once

#include <types.h>
#include <exec/process.h>

// yeah this is tied to linux but i am lazy. vfs behaves similarly
// after all.
#define UACCESS_EFAULT 14
#define UACCESS_ENAMETOOLONG 36

bool user_range_ok(uint64_t addr, size_t len);
int copy_from_user(process_t* proc, void* kdst, const void* usrc, size_t len);
int copy_to_user(process_t* proc, void* udst, const void* ksrc, size_t len);

int strncpy_from_user(process_t* proc, char* kdst, const char* usrc, size_t max);
