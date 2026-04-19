#pragma once

#define NULL ((void*)0)

typedef signed char int8;
typedef unsigned char uint8;

typedef signed short int16;
typedef unsigned short uint16;

typedef signed int int32;
typedef unsigned int uint32;

typedef signed long long int64;
typedef unsigned long long uint64;

typedef unsigned long size_t;
typedef unsigned long uintptr_t;  
typedef signed long   intptr_t;

typedef void (*function_ptr)();

typedef signed char int8_t; 
typedef unsigned char uint8_t;

typedef signed short int16_t; 
typedef unsigned short uint16_t;

typedef signed int int32_t;
typedef unsigned int uint32_t;

typedef signed long long int64_t;
typedef unsigned long long uint64_t;

#ifndef __cplusplus
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
        // W C23 bool to słowo kluczowe, nic nie rób
    #else
        // Dla starszych standardów (C99, C11)
        typedef _Bool bool;
        #define true 1
        #define false 0
    #endif
#endif
