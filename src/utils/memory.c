#include <stdlib.h>
#include <stddef.h>
#include "utils/memory.h"
#include "utils/log.h"

static size_t g_allocated = 0;

void mem_init(void) {
    g_allocated = 0;
}

void *mem_alloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr)
        g_allocated += size;
    else
        LOG_E("mem_alloc failed: %u bytes", (unsigned)size);
    return ptr;
}

void mem_free(void *ptr) {
    free(ptr);
}

size_t mem_available(void) {
    /* PSP heap is fixed at build time via PSP_HEAP_SIZE_KB.
       Return rough estimate based on tracked allocs. */
    return (8u * 1024u * 1024u) - g_allocated;
}
