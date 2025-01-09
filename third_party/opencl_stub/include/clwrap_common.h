#pragma once

#include <chrono>
#include <cmath>
#include <vector>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "CL/opencl.h"

/*************************************
 * * cl_qcom_perf_hint extension *
 * *************************************/
#define CL_PERF_HINT_NONE_QCOM 0
typedef cl_uint cl_perf_hint;
#define CL_CONTEXT_PERF_HINT_QCOM 0x40C2
/*cl_perf_hint*/
#define CL_PERF_HINT_HIGH_QCOM 0x40C3
#define CL_PERF_HINT_NORMAL_QCOM 0x40C4
#define CL_PERF_HINT_LOW_QCOM 0x40C5
/*************************************
 * * cl_qcom_priority_hint extension *
 * *************************************/
#define CL_PRIORITY_HINT_NONE_QCOM 0
typedef cl_uint cl_priority_hint;
#define CL_CONTEXT_PRIORITY_HINT_QCOM 0x40C9
/*cl_priority_hint*/
#define CL_PRIORITY_HINT_HIGH_QCOM 0x40CA
#define CL_PRIORITY_HINT_NORMAL_QCOM 0x40CB
#define CL_PRIORITY_HINT_LOW_QCOM 0x40CC


#define UNUSED(expr) do { (void)(expr); } while (0)

const char* get_error_string(cl_int error);

#define opencl_check(_expr)                                                \
    do {                                                                   \
        cl_int _err = (_expr);                                             \
        if (_err != CL_SUCCESS) {                                          \
            fprintf(stderr, "opencl error %d(%s)(%s at %s:%s:%d)\n", _err, \
                    get_error_string(_err), #_expr, __FILE__, __func__,    \
                    __LINE__);                                             \
            fflush(stderr);                                                \
            fflush(stdout);                                                \
            abort();                                                       \
        }                                                                  \
    } while (0)

void warmup();

/**
 * \warning the device should support image and uniform-work-group-size
 */

std::pair<cl_device_id, cl_platform_id> create_device2();

cl_device_id create_device();

cl_program build_program(cl_context ctx, cl_device_id dev,
                         const char* program_buffer);

/*!
 * \brief align *val* to be multiples of *align*
 * \param align required alignment, which must be power of 2
 */
template <typename T>
static inline T aligned_power2(T val, T align) {
    auto d = val & (align - 1);
    val += (align - d) & (align - 1);
    return val;
}


// vim: syntax=cpp.doxygen
