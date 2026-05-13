#pragma once
#include <stddef.h>

void   mem_init(void);
void  *mem_alloc(size_t size);
void   mem_free(void *ptr);
size_t mem_available(void);
