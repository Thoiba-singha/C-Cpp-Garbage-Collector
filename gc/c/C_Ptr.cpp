
#include "../gc.h"
#include <memory>
#include <iostream>
#include <cstring>

struct DebugDeleter {
    void operator()(char* ptr) const noexcept {
        std::cout << "[C++ backend] Freed memory @ "
            << static_cast<void*>(ptr) << "\n";
        delete[] ptr;
    }
};

// The real object with destructor (invisible to C)
struct LocalPtrCpp {
    void* raw = nullptr;
    std::shared_ptr<char> holder;

    ~LocalPtrCpp() {
        // When this dies → shared_ptr dies → DebugDeleter prints
    }
};

extern "C" {

    // allocate size bytes
    PtrBase gc_local_malloc(size_t size) {
        LocalPtrCpp cpp;
        cpp.holder = std::shared_ptr<char>(new char[size], DebugDeleter{});
        cpp.raw = cpp.holder.get();
        return *(PtrBase*)&cpp;
    }

    // allocate and zero memory
    PtrBase gc_local_calloc(size_t count, size_t size) {
        size_t total = count * size;
        LocalPtrCpp cpp;
        cpp.holder = std::shared_ptr<char>(new char[total], DebugDeleter{});
        std::memset(cpp.holder.get(), 0, total);
        cpp.raw = cpp.holder.get();
        return *(PtrBase*)&cpp;
    }

} // extern "C"

