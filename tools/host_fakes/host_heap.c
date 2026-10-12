#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "host_fakes.h"
#include <stdlib.h>

bool host_heap_fail_all;
bool (*host_heap_fail_hook)(size_t size, unsigned caps);

static bool fails(size_t size, unsigned caps)
{
    return host_heap_fail_all || (host_heap_fail_hook && host_heap_fail_hook(size, caps));
}

void *heap_caps_malloc(size_t size, unsigned caps) { return fails(size, caps) ? NULL : malloc(size); }
void *heap_caps_calloc(size_t count, size_t size, unsigned caps) { return fails(count * size, caps) ? NULL : calloc(count, size); }
void heap_caps_free(void *pointer) { free(pointer); }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 65536; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return 32768; }
bool esp_ptr_external_ram(const void *pointer) { (void)pointer; return false; }
