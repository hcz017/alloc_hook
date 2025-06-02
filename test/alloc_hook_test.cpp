#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/mman.h>
#include <malloc.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex>
#include <filesystem>
#include <sys/mman.h>
#include <linux/dma-heap.h>

#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <CL/cl.h>
#include "CL/cl_platform.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "util/gtest_utils.h"
#include "gles3jni.h"

#define DISALLOW_COPY_AND_ASSIGN(TypeName)      \
  TypeName(const TypeName&) = delete;           \
  void operator=(const TypeName&) = delete

typedef void (*checkpoint_func)(const char*);

namespace Checker {

bool verify_memory_info(const char* file_name, float host_mem, float dma_mem, float tot_mem) {
    printf("origin memory info(dumpsys/dmabuf_dump): host %f, dma %f, total %f\n", host_mem, dma_mem, tot_mem);
    const float max_diff_size = 30;
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>{fopen(file_name, "re"), fclose};
    if (fp == nullptr) {
        printf("open backtrace file failed\n");
        return false;
    }

    std::regex pattern(R"([a-z: ]+([0-9.]+)MB, [a-z: ]+([0-9.]+)MB, [a-z: ]+([0-9.]+)MB)");
    std::smatch match;
    char buffer[1024];

    float re_host_mem, re_dma_mem, re_tot_mem;
    if (fgets(buffer, sizeof(buffer), fp.get())) {
        std::string line(buffer);
        if (std::regex_search(line, match, pattern) && match.size() == 4) {
            re_host_mem = std::stof(match[1].str());
            re_dma_mem = std::stof(match[2].str());
            re_tot_mem = std::stof(match[3].str());
        }
    }

    double diff_host_mem = std::fabs(re_host_mem - host_mem);
    double diff_dma_mem = std::fabs(re_dma_mem - dma_mem);
    printf("regex memory info(from trace file): host %f, dma %f, total %f\n", re_host_mem, re_dma_mem, re_tot_mem);
    if ((diff_host_mem > max_diff_size) || (diff_dma_mem > max_diff_size)){
        printf("[error] diff_host_mem %f, diff_dma_mem %f, diff threshold %f \n",
               diff_host_mem, diff_dma_mem, max_diff_size);
    }

    return diff_host_mem < max_diff_size && diff_dma_mem < max_diff_size;
}

/**
*   mmap 分配的 CPU 内存，会被统计到 Private Other, 所以 meminfo 的 command 格式:
*       dumpsys meminfo <pid> | awk '/Native Heap:/ {print $3} /Graphics:/ {print $2} /Private Other:/ {print $3} /TOTAL PSS:/ {print $3}'       
*
*   dmabuf_dump 的 command 格式为:
*       dmabuf_dump <pid> | awk '/userspace_pss:/ {print $(NF-1)}'
*/
void parse_dump_info(std::vector<float*> val, const char* command) {
    auto pipe = std::unique_ptr<FILE, decltype(&fclose)>{popen(command, "r"), fclose};
    if (pipe == nullptr) {
        return ;
    }

    char* line = nullptr;
    size_t len = 0;
    for (int i = 0; i < val.size(); ++i) {
        if (getline(&line, &len, pipe.get()) > 0) {
            *val[i] = std::strtoul(line, nullptr, 10) / 1024.f;
        }
    }
    free(line);
}

/**
 * get memory info from dumpsys meminfo and dmabuf_dump
*/
void parse_memory_info(float* host, float* dma, float* mmap, float* total) {
    char command[256];
    sprintf(command, "dumpsys meminfo %d | awk '/Native Heap:/ {print $3} /Graphics:/ {print $2} "
                                        " /Private Other:/ {print $3} /TOTAL PSS:/ {print $3}'", getpid());
    parse_dump_info({host, dma, mmap, total}, command);

    if (*dma == 0) {
        sprintf(command, "dmabuf_dump %d | awk '/userspace_pss:/ {print $(NF-1)}'", getpid());
        parse_dump_info({dma}, command);
    }
}

void check_memory_info() {
    auto checkpoint = (checkpoint_func)dlsym(RTLD_DEFAULT, "checkpoint");
    ASSERT(checkpoint == nullptr, "pre-load liballoc_host.so failed\n");
    std::filesystem::path filePath = "/data/local/tmp/trace/memory_alloc_test.txt";
    // get meminfo with android system cmd
    float host_mem = 0.f, dma_mem = 0.f, mmap_mem = 0.f, total_mem = 0.f;
    // checkpoint dump trace
    checkpoint(filePath.c_str());
    Checker::parse_memory_info(&host_mem, &dma_mem, &mmap_mem, &total_mem);
    // check if two ways of memory info are equal
    EXPECT_TRUE(Checker::verify_memory_info(filePath.c_str(), host_mem + mmap_mem, dma_mem, total_mem));
}

}

namespace Memory {

void release(void *ptr, size_t) {
    free(ptr);
}

size_t qsize(const void* ptr) {
    return malloc_usable_size(ptr);
}

template<typename AllocFunc, typename... Args>
void* allocate(AllocFunc alloc, Args... args) {
    return alloc(args...);
}

template<typename AllocFunc, typename FreeFunc, typename QSizeFunc, typename... Args>
void run_alloc(AllocFunc alloc, FreeFunc release, QSizeFunc qsize, Args... args) {
    void* ptr = allocate(alloc, std::forward<Args>(args)...);
    ASSERT(ptr == nullptr, "Host memory allocate failed\n");
    // get ptr actual size
    size_t size = qsize(ptr);
    memset(ptr, 0, size);
    printf("host alloc ptr is %p, size is %zu\n", ptr, size);
    Checker::check_memory_info();
    release(ptr, size);
    Checker::check_memory_info();
}

std::pair<int, int> dma_alloc(size_t size) {
    int dma_node_fd = open("/dev/dma_heap/mtk_mm", O_RDONLY | O_CLOEXEC);
    if (dma_node_fd < 0) {
        dma_node_fd = open("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
    }

    ASSERT(dma_node_fd < 0, "failed to open dma heap: %s", strerror(errno));

    struct dma_heap_allocation_data heap_data = {.len = static_cast<uint64_t>(size),
                                                    .fd_flags = O_RDWR | O_CLOEXEC};
    int ret = ioctl(dma_node_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
    ASSERT(ret < 0, "failed to open dma heap node: %s", strerror(errno));

    if (heap_data.fd < 0) {
        close(dma_node_fd);
        dma_node_fd = -1;
        ASSERT(true, "failed to alloc dma heap: %s", strerror(errno));
    }
    return std::pair{dma_node_fd, heap_data.fd};
}

class OpenCLMemManager {
public:
    OpenCLMemManager() : context(nullptr), device(nullptr), queue(nullptr) {
        cl_int error;
        cl_platform_id platform;
        error = clGetPlatformIDs(1, &platform, NULL);
        CHECK_OPENCL_ERROR(error);

        error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
        CHECK_OPENCL_ERROR(error);

        context = clCreateContext(NULL, 1, &device, NULL, NULL, &error);
        CHECK_OPENCL_ERROR(error);

        queue = clCreateCommandQueue(context, device, 0, &error);
        CHECK_OPENCL_ERROR(error);
    }

    ~OpenCLMemManager() {
        if (queue) clReleaseCommandQueue(queue);
        if (context) clReleaseContext(context);
    }

    cl_mem createBuffer(cl_mem_flags flags, size_t size, void* ptr) {
        cl_int error;
        cl_mem buffer = clCreateBuffer(context, flags, size, ptr, &error);
        CHECK_OPENCL_ERROR(error);

        return buffer;
    }

    cl_mem createImage(cl_mem_flags flags, const cl_image_format& format, const cl_image_desc& desc, void* ptr) {
        cl_int error;
        cl_mem image = clCreateImage(context, flags, &format, &desc, ptr, &error);
        CHECK_OPENCL_ERROR(error);

        return image;
    }

    void* createSvm(cl_mem_flags flags, size_t size, cl_uint alignment) {
        cl_int error;
        void *svm_ptr = clSVMAlloc(context, flags, size, alignment);
        ASSERT(svm_ptr == nullptr, "Error: Failed to allocate SVM memory\n");

        return svm_ptr;
    }

    void freeSvm(void* ptr) {
        clSVMFree(context, ptr);
    }

    void releaseMem(cl_mem buf) {
        cl_int error = clReleaseMemObject(buf);
        CHECK_OPENCL_ERROR(error);
    }

    void* mapBuffer(cl_mem buf, size_t size) {
        cl_int error;
        void* ptr = clEnqueueMapBuffer(queue,buf,CL_TRUE,CL_MAP_READ | CL_MAP_WRITE, 0, size,
            0, NULL, NULL, &error);
        CHECK_OPENCL_ERROR(error);
        clFinish(queue);
        return ptr;
    }

    void* mapImage(cl_mem buf, size_t width, size_t height) {
        cl_int error;
        size_t origin[3] = {0, 0, 0}; // 起点
        size_t region[3] = {width, height, 1}; // 映射区域

        size_t rowPitch, slicePitch;
        void* ptr = clEnqueueMapImage(queue, buf, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE,
                                origin, region, &rowPitch, &slicePitch, 0, nullptr, nullptr, &error);
        CHECK_OPENCL_ERROR(error);
        clFinish(queue);
        return ptr;
    }

    void unmapMem(cl_mem buf, void* ptr) {
        cl_int error = clEnqueueUnmapMemObject(queue, buf, ptr, 0, NULL, NULL);
        CHECK_OPENCL_ERROR(error);
        clFinish(queue);
    }

private:
    cl_context context;
    cl_device_id device;
    cl_command_queue queue;

    DISALLOW_COPY_AND_ASSIGN(OpenCLMemManager);
};

}


TEST(BasicFunc, signal) {
    EXPECT_EQ(kill(getpid(), 33), 0);
}

TEST(BasicFunc, checkpoint) {
    auto checkpoint = (checkpoint_func)dlsym(RTLD_DEFAULT, "checkpoint");
    ASSERT(checkpoint == nullptr, "pre-load liballoc_host.so failed\n");
    std::filesystem::path filePath = "/data/local/tmp/trace/check_point_test.txt";
    checkpoint(filePath.c_str());
    
    EXPECT_TRUE(std::filesystem::exists(filePath));
}

TEST(HostAlloc, malloc) {
    const size_t size = 37 * 1024 * 1024;
    Memory::run_alloc(malloc, Memory::release, Memory::qsize, size);
}

TEST(HostAlloc, calloc) {
    const size_t size = 48 * 1024 * 1024;
    Memory::run_alloc(calloc, Memory::release, Memory::qsize, 3, size);
}

TEST(HostAlloc, realloc) {
    const size_t size = 11 * 1024 * 1024;
    void* ptr = malloc(size);
    Memory::run_alloc(realloc, Memory::release, Memory::qsize, ptr, size * 2);
}

TEST(HostAlloc, memalign) {
    const size_t size = 59 * 1024 * 1024;
    Memory::run_alloc(memalign, Memory::release, Memory::qsize, 64, size);
}

TEST(HostAlloc, posix_memalign) {
    void* ptr = nullptr;
    const size_t align = 64;
    const size_t size = 72 * 1024 * 1024;

    auto posix_alloc = [&]() -> void* {
        int ret = posix_memalign(&ptr, align, size);
        if (ret != 0) {
            return nullptr;
        }
        return ptr;
    };

    Memory::run_alloc(posix_alloc, Memory::release, Memory::qsize);
}

TEST(HostAlloc, mmap) {
    void* addr = NULL;
    const size_t size = 63 * 1024 * 1024;
    int prot = PROT_READ | PROT_WRITE;
    int flag = MAP_ANON | MAP_PRIVATE | MAP_POPULATE;
    int fd = -1;
    off_t offset = 0;
    auto qsize = [&](const void* ptr) {return size;};
    Memory::run_alloc(mmap, munmap, qsize, addr, size, prot, flag, fd, offset);
}

TEST(DmaAlloc, ioctl) {
    const size_t size = 79 * 1024 * 1024;
    std::pair<int, int> node = Memory::dma_alloc(size);

    auto dmaAlloc = [&]() {
        return mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, node.second, 0);
    };
    auto damFree = [&] (void* ptr, size_t size) {
        munmap(ptr, size);
        close(node.second);
        close(node.first);
    };
    auto qsize = [&](const void* ptr) {return size;};

    Memory::run_alloc(dmaAlloc, damFree, qsize);
}

TEST(OpenCLAlloc, clcreatebuffer) {
    const size_t size = 128 * 1024 * 1024;

    Memory::OpenCLMemManager test;
    cl_mem buf = test.createBuffer(CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, size, NULL);
    show_clmem_Info(buf);

    auto clAlloc = [&]() {
        return test.mapBuffer(buf, size);
    };
    auto clFree = [&] (void* ptr, size_t) {
        test.unmapMem(buf, ptr);
        test.releaseMem(buf);
    };
    auto qsize = [&](const void* ptr) {return size;};

    Memory::run_alloc(clAlloc, clFree, qsize);
}

TEST(OpenCLAlloc, clcreateimage) {
    cl_image_format format = {CL_RGBA, CL_UNSIGNED_INT8};
    cl_image_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = 4096;     
    desc.image_height = 3072;
    desc.image_row_pitch = 0;
    const size_t size = 4096 * 3072 * 4 * sizeof(uint8_t);

    Memory::OpenCLMemManager test;
    cl_mem buf = test.createImage(CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, format, desc, nullptr);
    show_clmem_Info(buf);

    auto clAlloc = [&]() ->void* {
        return test.mapImage(buf, desc.image_width, desc.image_height);
    };
    auto clFree = [&] (void* ptr, size_t) {
        test.unmapMem(buf, ptr);
        test.releaseMem(buf);
    };
    auto qsize = [&](const void* ptr) {return size;};

    Memory::run_alloc(clAlloc, clFree, qsize);
}

TEST(OpenCLAlloc, clsvmalloc) {
    const size_t align = 64;
    const size_t size = 50 * 1024 * 1024;

    Memory::OpenCLMemManager test;
    auto clAlloc = [&](size_t align) ->void* {
        return test.createSvm(CL_MEM_READ_WRITE, size, align);
    };
    auto clFree = [&] (void* ptr, size_t) {
        test.freeSvm(ptr);
    };
    auto qsize = [&](const void* ptr) {return size;};

    Memory::run_alloc(clAlloc, clFree, qsize, align);
}

TEST(OpenGLAlloc, render) {
    const char* str = "alloc hook test function opengl_render";
    printf("gtest: %s\n", str);

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        std::cerr << "Unable to open connection to local windowing system" << std::endl;
    }
    Checker::check_memory_info();

    EGLint majorVersion, minorVersion;
    if (!eglInitialize(display, &majorVersion, &minorVersion)) {
        std::cerr << "Unable to initialize EGL" << std::endl;
    }
    Checker::check_memory_info();

    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
        std::cerr << "Unable to choose EGL config" << std::endl;
    }
    Checker::check_memory_info();

    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        std::cerr << "Unable to create EGL context" << std::endl;
    }
    Checker::check_memory_info();

    EGLint pbufferAttribs[] = {
        EGL_WIDTH, 800,
        EGL_HEIGHT, 600,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttribs);
    if (surface == EGL_NO_SURFACE) {
        std::cerr << "Unable to create EGL surface" << std::endl;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        std::cerr << "Unable to make EGL context current" << std::endl;
    }
    Checker::check_memory_info();

    Renderer* renderer = createES2Renderer();
    if (!renderer) {
        std::cerr << "Failed to create renderer" << std::endl;
    }
    renderer->render();
    renderer->resize(4096, 3072);
    renderer->render();
    Checker::check_memory_info();

    delete renderer;
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    Checker::check_memory_info();
}

int main(int argc, char** argv) {
    /*
    *  依赖列表中, 可以替换 gtest 为 gtest_main (gtest 实现的 main 函数), 这里为了写提示（未来可能增加其他功能）自实现了 main
    *
    *  mmap64 作为 ioctl 的拓展，不单独提供测试
    *
    *  liballoc_hook.so 自身会占用一些空间, 在 host 和 cl 测试中占用大概 7MB, 在 opengl 测试中占用大概 20MB
    *  可以关闭 pre_load, 并插入锚点进行测试, 锚点代码如下：
    *      char command[256];
    *      sprintf(command, "dumpsys meminfo %d", getpid());
    *      system(command);
    */
    ::testing::InitGoogleTest(&argc, argv);
    // ::testing::GTEST_FLAG(filter) = "OpenGLAlloc.*";
    return RUN_ALL_TESTS();
}