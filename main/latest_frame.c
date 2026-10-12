#include "latest_frame.h"
#include <string.h>

enum { DIRTY = 4, INDEX = 3 };

void latest_frame_seed(latest_frame_t *f, const uint8_t *pixels)
{
    for (unsigned i = 0; i < 3; ++i) memcpy(f->pixels[i], pixels, GB_PACKED_BYTES);
    f->front = 0; f->back = 2;
    atomic_store(&f->middle, 1);
}

void latest_frame_publish(latest_frame_t *f, const uint8_t *pixels)
{
    memcpy(f->pixels[f->back], pixels, GB_PACKED_BYTES);
    f->back = atomic_exchange_explicit(&f->middle, f->back | DIRTY,
                                      memory_order_acq_rel) & INDEX;
}

void latest_frame_copy(latest_frame_t *f, uint8_t *pixels)
{
    if (atomic_load_explicit(&f->middle, memory_order_acquire) & DIRTY)
        f->front = atomic_exchange_explicit(&f->middle, f->front,
                                           memory_order_acq_rel) & INDEX;
    memcpy(pixels, f->pixels[f->front], GB_PACKED_BYTES);
}
