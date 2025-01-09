#include "clwrap_common.h"
const char* get_error_string(cl_int error) {
#define ERROR(_err) \
    case _err:      \
        return #_err;

    switch (error) {
        ERROR(CL_SUCCESS)
        ERROR(CL_DEVICE_NOT_FOUND)
        ERROR(CL_DEVICE_NOT_AVAILABLE)
        ERROR(CL_COMPILER_NOT_AVAILABLE)
        ERROR(CL_MEM_OBJECT_ALLOCATION_FAILURE)
        ERROR(CL_OUT_OF_RESOURCES)
        ERROR(CL_OUT_OF_HOST_MEMORY)
        ERROR(CL_PROFILING_INFO_NOT_AVAILABLE)
        ERROR(CL_MEM_COPY_OVERLAP)
        ERROR(CL_IMAGE_FORMAT_MISMATCH)
        ERROR(CL_IMAGE_FORMAT_NOT_SUPPORTED)
        ERROR(CL_BUILD_PROGRAM_FAILURE)
        ERROR(CL_MAP_FAILURE)
        ERROR(CL_MISALIGNED_SUB_BUFFER_OFFSET)
        ERROR(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST)
        ERROR(CL_COMPILE_PROGRAM_FAILURE)
        ERROR(CL_LINKER_NOT_AVAILABLE)
        ERROR(CL_LINK_PROGRAM_FAILURE)
        ERROR(CL_DEVICE_PARTITION_FAILED)
        ERROR(CL_KERNEL_ARG_INFO_NOT_AVAILABLE)
        ERROR(CL_INVALID_VALUE)
        ERROR(CL_INVALID_DEVICE_TYPE)
        ERROR(CL_INVALID_PLATFORM)
        ERROR(CL_INVALID_DEVICE)
        ERROR(CL_INVALID_CONTEXT)
        ERROR(CL_INVALID_QUEUE_PROPERTIES)
        ERROR(CL_INVALID_COMMAND_QUEUE)
        ERROR(CL_INVALID_HOST_PTR)
        ERROR(CL_INVALID_MEM_OBJECT)
        ERROR(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)
        ERROR(CL_INVALID_IMAGE_SIZE)
        ERROR(CL_INVALID_SAMPLER)
        ERROR(CL_INVALID_BINARY)
        ERROR(CL_INVALID_BUILD_OPTIONS)
        ERROR(CL_INVALID_PROGRAM)
        ERROR(CL_INVALID_PROGRAM_EXECUTABLE)
        ERROR(CL_INVALID_KERNEL_NAME)
        ERROR(CL_INVALID_KERNEL_DEFINITION)
        ERROR(CL_INVALID_KERNEL)
        ERROR(CL_INVALID_ARG_INDEX)
        ERROR(CL_INVALID_ARG_VALUE)
        ERROR(CL_INVALID_ARG_SIZE)
        ERROR(CL_INVALID_KERNEL_ARGS)
        ERROR(CL_INVALID_WORK_DIMENSION)
        ERROR(CL_INVALID_WORK_GROUP_SIZE)
        ERROR(CL_INVALID_WORK_ITEM_SIZE)
        ERROR(CL_INVALID_GLOBAL_OFFSET)
        ERROR(CL_INVALID_EVENT_WAIT_LIST)
        ERROR(CL_INVALID_EVENT)
        ERROR(CL_INVALID_OPERATION)
        ERROR(CL_INVALID_GL_OBJECT)
        ERROR(CL_INVALID_BUFFER_SIZE)
        ERROR(CL_INVALID_MIP_LEVEL)
        ERROR(CL_INVALID_GLOBAL_WORK_SIZE)
        ERROR(CL_INVALID_PROPERTY)
        ERROR(CL_INVALID_IMAGE_DESCRIPTOR)
        ERROR(CL_INVALID_COMPILER_OPTIONS)
        ERROR(CL_INVALID_LINKER_OPTIONS)
        ERROR(CL_INVALID_DEVICE_PARTITION_COUNT)

        default:
            return "unknown error";
    }
}
#define ASSERT_EQ(actual, expect)                                    \
    if (std::fabs((float)(actual) - (float)(expect)) > 1e-3) {       \
        fprintf(stderr, "expect: %f actual: %f \n", (float)(expect), \
                (float)(actual));                                    \
        exit(1);                                                     \
    }

void warmup() {
    std::vector<int> data(1024000, 1);
    int sum = 0;
    for (size_t i = 0; i < data.size(); i++) {
        sum += data[i];
    }
    ASSERT_EQ(sum, data.size());
}
#undef ASSERT_EQ
std::pair<cl_device_id, cl_platform_id> create_device2() {
    cl_uint nr_platforms = 0;

    opencl_check(clGetPlatformIDs(0, nullptr, &nr_platforms));
    assert(nr_platforms > 0);

    std::vector<cl_platform_id> platform_ids(nr_platforms);
    opencl_check(clGetPlatformIDs(nr_platforms, platform_ids.data(), nullptr));

    cl_uint nr_devices = 0;
    std::vector<cl_device_id> device_ids;
    cl_platform_id platform_id = 0;
    for (auto pid : platform_ids) {
        opencl_check(clGetDeviceIDs(pid, CL_DEVICE_TYPE_GPU, 0, nullptr,
                                    &nr_devices));
        if (!nr_devices) {
            continue;
        }

        device_ids.resize(nr_devices);
        opencl_check(clGetDeviceIDs(pid, CL_DEVICE_TYPE_GPU, nr_devices,
                                    device_ids.data(), nullptr));
        platform_id = pid;
    }
    //! just return the first selected GPU
    return {device_ids[0], platform_id};
}

cl_device_id create_device() {
    cl_uint nr_platforms = 0;

    opencl_check(clGetPlatformIDs(0, nullptr, &nr_platforms));
    assert(nr_platforms > 0);

    std::vector<cl_platform_id> platform_ids(nr_platforms);
    opencl_check(clGetPlatformIDs(nr_platforms, platform_ids.data(), nullptr));

    cl_uint nr_devices = 0;
    std::vector<cl_device_id> device_ids;
    for (auto pid : platform_ids) {
        opencl_check(clGetDeviceIDs(pid, CL_DEVICE_TYPE_GPU, 0, nullptr,
                                    &nr_devices));
        if (!nr_devices) {
            continue;
        }

        device_ids.resize(nr_devices);
        opencl_check(clGetDeviceIDs(pid, CL_DEVICE_TYPE_GPU, nr_devices,
                                    device_ids.data(), nullptr));
    }
    //! just return the first selected GPU
    return device_ids[0];
}

cl_program build_program(cl_context ctx, cl_device_id dev,
                         const char* program_buffer) {
    cl_program program;
    char *program_log;
    size_t program_size, log_size;
    int err;

    program_size = strlen(program_buffer);

    program = clCreateProgramWithSource(ctx, 1, (const char**)&program_buffer,
                                        &program_size, &err);
    opencl_check(err);

    const char* compile_options = "-cl-fast-relaxed-math -cl-mad-enable";
    err = clBuildProgram(program, 1, &dev, compile_options, NULL, NULL);
    opencl_check(err);
    
    if (err < 0) {
        clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 0, NULL,
                              &log_size);
        program_log = (char*)malloc(log_size + 1);
        program_log[log_size] = '\0';
        clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, log_size + 1,
                              program_log, NULL);
        printf("%s\n", program_log);
        free(program_log);
        exit(1);
    }

    return program;
}
