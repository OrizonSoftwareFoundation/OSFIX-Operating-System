
#include <stddef.h>
#include "../includes/heapalloc/tlsf.h"

extern tlsf_t kernel_tlsf_pool; 

void *malloc(size_t size) {
    return tlsf_malloc(kernel_tlsf_pool, size);
}

void *calloc(size_t n, size_t size) {
    void *ptr = tlsf_malloc(kernel_tlsf_pool, n * size);
    if (ptr) __builtin_memset(ptr, 0, n * size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    return tlsf_realloc(kernel_tlsf_pool, ptr, size);
}

void free(void *ptr) {
    tlsf_free(kernel_tlsf_pool, ptr);
}