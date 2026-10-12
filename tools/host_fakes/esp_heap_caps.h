#pragma once
#include <stddef.h>

#define MALLOC_CAP_SPIRAM 1u
#define MALLOC_CAP_8BIT 2u
#define MALLOC_CAP_INTERNAL 4u
#define MALLOC_CAP_DMA 8u

void *heap_caps_malloc(size_t size, unsigned caps);
void *heap_caps_calloc(size_t count, size_t size, unsigned caps);
void heap_caps_free(void *pointer);
size_t heap_caps_get_free_size(unsigned caps);
size_t heap_caps_get_largest_free_block(unsigned caps);
