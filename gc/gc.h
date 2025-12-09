#pragma once

#ifdef __cplusplus
   #include "../gc/cpp/Cpp_Ptr.hpp"
extern "C" {
#endif

#include <stddef.h>


    // C-visible minimal struct (C++ expands it internally)
    typedef struct PtrBase {
        void* raw;
    }PtrBase;

    // Base allocators
    PtrBase gc_local_malloc(size_t size);
    PtrBase gc_local_calloc(size_t count, size_t size);

    // ----------------------------------------------
    // High-Level Typed API for C (NO casts)
    // ----------------------------------------------
#define Ptr(T) T*

// Allocate one object of type T
#define gc_new(T) \
    ((T*)gc_local_malloc(sizeof(T)).raw)

// Allocate array (typed)
#define gc_new_array(T, count) \
    ((T*)gc_local_calloc((count), sizeof(T)).raw)

// malloc-style
#define gc_malloc(size) \
    (gc_local_malloc(size).raw)

// calloc-style
#define gc_calloc(count, size) \
    (gc_local_calloc((count), (size)).raw)


#ifdef __cplusplus
}
#endif
