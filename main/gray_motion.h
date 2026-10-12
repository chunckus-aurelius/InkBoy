#pragma once
#include <stdbool.h>
#include <stdint.h>
#define GRAY_MOTION_TRAVEL_US 5000000LL
#define GRAY_MOTION_LEAD_US 1000000LL
#define GRAY_MOTION_CYCLE_US 6000000LL
#define GRAY_MOTION_WIDTH 12u
#define GRAY_MOTION_MAX_X (160u-GRAY_MOTION_WIDTH)
/* 0=prepare, 1=moving, 2=right-end inspection; pause is owned by the caller. */
static inline unsigned gray_motion_state(int64_t elapsed) {
    if (elapsed < GRAY_MOTION_LEAD_US) return 0;
    return (elapsed-GRAY_MOTION_LEAD_US)%GRAY_MOTION_CYCLE_US < GRAY_MOTION_TRAVEL_US ? 1 : 2;
}
static inline unsigned gray_motion_x(int64_t elapsed) {
    if (elapsed < GRAY_MOTION_LEAD_US) return 0;
    int64_t phase=(elapsed-GRAY_MOTION_LEAD_US)%GRAY_MOTION_CYCLE_US;
    if (phase >= GRAY_MOTION_TRAVEL_US) return GRAY_MOTION_MAX_X;
    return (unsigned)(phase*GRAY_MOTION_MAX_X/GRAY_MOTION_TRAVEL_US);
}
static inline unsigned gray_motion_patch(unsigned background, unsigned lane) {
    return lane < background ? lane : lane+1;
}
/* Native DMG packed format: white=0, black=3, left pixel in low two bits. */
void gray_motion_render(uint8_t *packed, unsigned x);
