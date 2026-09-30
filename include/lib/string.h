#pragma once 

#include <types.h>

extern char* strcpy(char* dest, const char* src);
extern int strlen(const char* str);
extern void str_reverse(char *str, int len);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, uint64_t n);
