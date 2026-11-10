#pragma once

#include <types.h>

typedef struct cmdline cmdline_t;

#define CMDLINE_OK                  0
#define CMDLINE_ERR_NULL_POINTER   -1
#define CMDLINE_ERR_UNTERMINATED   -2  
#define CMDLINE_ERR_INVALID_NAME   -3  
#define CMDLINE_ERR_UNEXPECTED_QUOTE -4 

int cmdline_parse(const char* input, cmdline_t** out);
void cmdline_free(cmdline_t* cl); 

size_t cmdline_count(const cmdline_t* cl);
const char* cmdline_name_at(const cmdline_t* cl, size_t index);
const char* cmdline_value_at(const cmdline_t* cl, size_t index); 

const char* cmdline_get(const cmdline_t* cl, const char* name);
int cmdline_has(const cmdline_t* cl, const char* name);

const char* cmdline_strerror(int code);
