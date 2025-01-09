#include <cstdio>
#include "CL/cl.h"
#include "gtest_utils.h"

const char* cl_error_string(cl_int error) {
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

void show_clmem_Info(cl_mem buf) {
    cl_mem_object_type mem_type;
    cl_int error;

    error = clGetMemObjectInfo(buf, CL_MEM_TYPE, sizeof(mem_type), &mem_type, NULL);
    CHECK_OPENCL_ERROR(error);

    switch (mem_type) {
        case CL_MEM_OBJECT_BUFFER:
            printf("Memory object type: CL_MEM_OBJECT_BUFFER\n");
            break;
        case CL_MEM_OBJECT_IMAGE2D:
            printf("Memory object type: CL_MEM_OBJECT_IMAGE2D\n");
            break;
        case CL_MEM_OBJECT_IMAGE3D:
            printf("Memory object type: CL_MEM_OBJECT_IMAGE3D\n");
            break;
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
            printf("Memory object type: CL_MEM_OBJECT_IMAGE2D_ARRAY\n");
            break;
        case CL_MEM_OBJECT_IMAGE1D:
            printf("Memory object type: CL_MEM_OBJECT_IMAGE1D\n");
            break;
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
            printf("Memory object type: CL_MEM_OBJECT_IMAGE1D_ARRAY\n");
            break;
        default:
            printf("Unknown memory object type: %d\n", mem_type);
            break;
    }

    size_t mem_size;
    error = clGetMemObjectInfo(buf, CL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
    CHECK_OPENCL_ERROR(error);
    printf("Memory object size: %zu bytes\n", mem_size);
    
    if (mem_type == CL_MEM_OBJECT_IMAGE2D || mem_type == CL_MEM_OBJECT_IMAGE3D || mem_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        cl_image_format image_format;
        size_t image_width, image_height, image_depth, image_row_pitch;
        error = clGetImageInfo(buf, CL_IMAGE_FORMAT, sizeof(image_format), &image_format, NULL);
        CHECK_OPENCL_ERROR(error);

        error = clGetImageInfo(buf, CL_IMAGE_WIDTH, sizeof(image_width), &image_width, NULL);
        CHECK_OPENCL_ERROR(error);

        error = clGetImageInfo(buf, CL_IMAGE_HEIGHT, sizeof(image_height), &image_height, NULL);
        CHECK_OPENCL_ERROR(error);

        error = clGetImageInfo(buf, CL_IMAGE_DEPTH, sizeof(image_depth), &image_depth, NULL);
        CHECK_OPENCL_ERROR(error);

        error = clGetImageInfo(buf, CL_IMAGE_ROW_PITCH, sizeof(image_row_pitch), &image_row_pitch, NULL);
        CHECK_OPENCL_ERROR(error);

        switch (image_format.image_channel_order) {
            case CL_R:
                printf("Image Format: CL_R\n");
                break;
            case CL_A:
                printf("Image Format: CL_A\n");
                break;
            case CL_RG:
                printf("Image Format: CL_RG\n");
                break;
            case CL_RA:
                printf("Image Format: CL_RA\n");
                break;
            case CL_RGB:
                printf("Image Format: CL_RGB\n");
                break;
            case CL_RGBA:
                printf("Image Format: CL_RGBA\n");
                break;
            case CL_BGRA:
                printf("Image Format: CL_BGRA\n");
                break;
            case CL_ARGB:
                printf("Image Format: CL_ARGB\n");
                break;
            default:
                printf("Image Format Code: %#X, Please check cl definition\n", image_format.image_channel_order);
                break;
        }

        switch (image_format.image_channel_data_type) {
            case CL_SNORM_INT8: 
                printf("Channel Type: CL_SNORM_INT8\n"); 
                break;
            case CL_SNORM_INT16: 
                printf("Channel Type: CL_SNORM_INT16\n"); 
                break;
            case CL_UNORM_INT8: 
                printf("Channel Type: CL_UNORM_INT8\n"); 
                break;
            case CL_UNORM_INT16: 
                printf("Image Channel Type: CL_UNORM_INT16\n"); 
                break;
            case CL_SIGNED_INT8: 
                printf("Image Channel Type: CL_SIGNED_INT8\n"); 
                break;
            case CL_SIGNED_INT16: 
                printf("Image Channel Type: CL_SIGNED_INT16\n"); 
                break;
            case CL_SIGNED_INT32: 
                printf("Image Channel Type: CL_SIGNED_INT32\n"); 
                break;
            case CL_UNSIGNED_INT8: 
                printf("Image Channel Type: CL_UNSIGNED_INT8\n"); 
                break;
            case CL_UNSIGNED_INT16: 
                printf("Image Channel Type: CL_UNSIGNED_INT16\n"); 
                break;
            case CL_UNSIGNED_INT32: 
                printf("Image Channel Type: CL_UNSIGNED_INT32\n"); 
                break;
            case CL_HALF_FLOAT: 
                printf("Image Channel Type: CL_HALF_FLOAT\n"); 
                break;
            case CL_FLOAT: 
                printf("Image Channel Type: CL_FLOAT\n"); 
                break;
            default: 
                printf("Image Channel Type: %#X, Please check cl definition\n", image_format.image_channel_data_type);
                break;
        }

        printf("Image Info: Width %zu, Height %zu, Depth %zu, Row Pitch %zu\n",
                        image_width, image_height, image_depth, image_row_pitch);
    }
}