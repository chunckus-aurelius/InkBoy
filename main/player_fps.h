#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Changed game images acknowledged by the display, not emulator frames or
 * repeated completion scans. Overlay-only updates never contribute. */
typedef struct {
    int64_t window_start;
    unsigned frames, tenths;
} player_fps_t;
static inline void player_fps_reset(player_fps_t *p, int64_t now) {
    *p=(player_fps_t){.window_start=now};
}
static inline void player_fps_presented(player_fps_t *p) { ++p->frames; }
static inline bool player_fps_poll(player_fps_t *p, int64_t now) {
    int64_t elapsed=now-p->window_start;
    if (elapsed<1000000) return false;
    uint64_t value=((uint64_t)p->frames*10000000u+(uint64_t)elapsed/2)/(uint64_t)elapsed;
    unsigned next=value>999 ? 999 : (unsigned)value;
    bool changed=next!=p->tenths;
    p->tenths=next; p->frames=0; p->window_start=now;
    return changed;
}
