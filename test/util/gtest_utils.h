#pragma once
#include <stdio.h>
#include <stdlib.h>

#include <CL/cl.h>

void show_clmem_Info(cl_mem buf);
const char* cl_error_string(cl_int error);

#define ASSERT(cond, fmt...)        \
    do {                            \
        if (cond) {                 \
            printf(fmt);            \
            __builtin_trap();       \
        }                           \
    } while (0)

#define CHECK_OPENCL_ERROR(_expr)                                                       \
    do {                                                                                \
        cl_int _err = (_expr);                                                          \
        if (_err != CL_SUCCESS) {                                                       \
            fprintf(stderr, "OpenCL error %s at %s:%s:%d\n",                            \
                    cl_error_string(_err), __FILE__, __func__, __LINE__);               \
            fflush(stderr);                                                             \
            fflush(stdout);                                                             \
            abort();                                                                    \
        }                                                                               \
    } while (0)