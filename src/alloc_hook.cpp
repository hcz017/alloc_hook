#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstddef>

#include <dlfcn.h>
#include <fcntl.h>

#include "DebugData.h"
#include "PointerData.h"
#include "malloc_debug.h"
#include "memory_hook.h"

#define RESOLVE(name)                                                              \
    do {                                                                           \
        if (m_sys_##name == nullptr) {                                             \
            void* handle = dlopen("libc.so", RTLD_LAZY);                           \
            if (handle) {                                                          \
                auto addr = dlsym(handle, #name);                                  \
                if (addr) {                                                        \
                    m_sys_##name = reinterpret_cast<decltype(m_sys_##name)>(addr); \
                }                                                                  \
                dlclose(handle);                                                   \
            }                                                                      \
        }                                                                          \
    } while (0)

struct InitState {
    InitState() { allocHook_setup = true; }
    ~InitState() { allocHook_setup = false; }
    static volatile bool allocHook_setup;
};
volatile bool InitState::allocHook_setup = false;

class AllocHook {
public:
    AllocHook() {
        InitState state;
        void* ptr[2] = {&Db_storage, &Pd_storage};
        // 初始化传入 debug data 和 pointer data
        debug_initialize(ptr);
    }
    ~AllocHook() { debug_finalize(); }

    void* malloc(size_t size) { return debug_malloc(size); }
    void free(void* ptr) { debug_free(ptr); }
    void* calloc(size_t a, size_t b) { return debug_calloc(a, b); }
    void* realloc(void* ptr, size_t size) { return debug_realloc(ptr, size); }
    int posix_memalign(void** ptr, size_t alignment, size_t size) {
        return debug_posix_memalign(ptr, alignment, size);
    }
    void* memalign(size_t alignment, size_t bytes) { return debug_memalign(alignment, bytes); }
    void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
        return debug_mmap(addr, size, prot, flags, fd, offset);
    }
    int munmap(void* addr, size_t size) { return debug_munmap(addr, size); }
    int ioctl(int fd, int request, void* arg) { return debug_ioctl(fd, request, arg); }
    int close(int fd) { return debug_close(fd); }
    void* mmap64(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
        return debug_mmap64(addr, size, prot, flags, fd, offset);
    }

    void checkpoint(const char* file_name) { return debug_dump_heap(file_name); }

    static AllocHook& inst();

private:
    static std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type Db_storage;
    static std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type
            Pd_storage;
};
std::aligned_storage<sizeof(DebugData), alignof(DebugData)>::type AllocHook::Db_storage;
std::aligned_storage<sizeof(PointerData), alignof(PointerData)>::type
        AllocHook::Pd_storage;

AllocHook& AllocHook::inst() {
    static AllocHook hook;
    return hook;
}

static volatile bool in_preinit_phase = true;
// 该代码使用 GCC 的 __attribute__((constructor)) 特性，在程序启动时自动调用
// mark_init_done 函数，并将其构造阶段设为 201。函数的作用是将全局变量 in_preinit_phase
// 设为 false，表示预初始化阶段已完成。
__attribute__((constructor(201))) void mark_init_done() {
    in_preinit_phase = false;
}

extern "C" {
// 程序初始化会间接调用 malloc 和 free
void* malloc(size_t size) {
    RESOLVE(malloc);
    if (InitState::allocHook_setup) {
        return m_sys_malloc(size);
    }
    return AllocHook::inst().malloc(size);
}

void free(void* ptr) {
    RESOLVE(free);
    if (InitState::allocHook_setup) {
        return m_sys_free(ptr);
    }
    return AllocHook::inst().free(ptr);
}

// calloc 和 realloc 属于用户级函数
void* calloc(size_t a, size_t b) {
    RESOLVE(calloc);
    if (InitState::allocHook_setup) {
        return m_sys_calloc(a, b);
    }
    return AllocHook::inst().calloc(a, b);
}

void* realloc(void* ptr, size_t size) {
    RESOLVE(realloc);
    if (InitState::allocHook_setup) {
        return m_sys_realloc(ptr, size);
    }
    return AllocHook::inst().realloc(ptr, size);
}

void* memalign(size_t alignment, size_t bytes)  {
    RESOLVE(memalign);
    return AllocHook::inst().memalign(alignment, bytes);
}

// 进程初始化 和 debug init 的过程不应该调用 posix_memalign
int posix_memalign(void** ptr, size_t alignment, size_t size) {
    RESOLVE(memalign);
    RESOLVE(posix_memalign);
    return AllocHook::inst().posix_memalign(ptr, alignment, size);
}

// 程序初始化时会调用 mmap, 类的构造函数如果包含内存申请，可能也会调用 mmap
void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
    if (in_preinit_phase || InitState::allocHook_setup) {
        return (void*)syscall(SYS_mmap, addr, size, prot, flags, fd, offset);
    }
    void* result = AllocHook::inst().mmap(addr, size, prot, flags, fd, offset);
    return result;
}

int munmap(void* addr, size_t size) {
    if (in_preinit_phase || InitState::allocHook_setup) {
        return (int)syscall(SYS_munmap, addr, size);
    }
    return AllocHook::inst().munmap(addr, size);
}

int ioctl(int fd, int request, ...) {
    // 这段代码用于处理可变参数列表：
    // va_list ap; 定义一个指向可变参数的指针；
    // va_start(ap, request); 初始化指针，request是最后一个固定参数；
    // void* arg = va_arg(ap, void*); 获取下一个参数（类型为void*）；
    // va_end(ap); 清理参数指针。
    va_list ap;
    va_start(ap, request);
    void* arg = va_arg(ap, void*);
    va_end(ap);

    return AllocHook::inst().ioctl(fd, request, arg);
}

int close(int fd) {
    return AllocHook::inst().close(fd);
}

/*
参数类型：

mmap 使用 off_t 类型表示偏移量，通常是 32 位，在某些系统上可能不支持大文件。
mmap64 使用 off64_t 类型表示偏移量，明确为 64 位，支持大于 2GB 的文件。
兼容性：

在 64 位系统上，mmap 和 mmap64 通常等价，因为 off_t 默认是 64 位。
在 32 位系统上，mmap64 是必要的，以支持大文件映射。
POSIX 标准：

mmap64 是 POSIX 指定的扩展接口，用于确保跨平台的大文件支持。
*/
void* mmap64(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
    if (in_preinit_phase || InitState::allocHook_setup) {
        return (void*)syscall(SYS_mmap, addr, size, prot, flags, fd, offset);
    }
    void* result = AllocHook::inst().mmap64(addr, size, prot, flags, fd, offset);
    return result;
}

void checkpoint(const char* file_name) {
    AllocHook::inst().checkpoint(file_name);
}
}