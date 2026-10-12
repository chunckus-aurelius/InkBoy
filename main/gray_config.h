#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Stable versioned NVS representation. 0=automatic extra scan, 1=one/slot. */
typedef struct {
    uint8_t version, dose[4][4], retarget_gain, scan_policy;
} gray_config_t;
#define GRAY_CONFIG_DEFAULT {1, {{0,2,4,7},{4,0,1,6},{4,1,0,5},{7,3,2,0}}, 2, 0}
#define GRAY_HOLDS_DEFAULT 0xc667u
#define GRAY_STRENGTH_DEFAULT 0u
#define GRAY_IMMEDIATE_DEFAULT 1u
#define GRAY_DOSE_MAX 12u
/* Bit (source*4 + target): stop promptly in the second scan. Unchanged shade entries always HOLD. */
#define GRAY_HOLD_DIAGONAL 0x8421u
#define GRAY_HOLD_ALL 0xffffu
static inline bool gray_hold_valid(unsigned mask) {
    return mask <= GRAY_HOLD_ALL && (mask & GRAY_HOLD_DIAGONAL) == GRAY_HOLD_DIAGONAL;
}
_Static_assert(sizeof(gray_config_t) == 19, "Stable grayscale settings layout");
static inline unsigned gray_retarget_percent(const gray_config_t *c) {
    return 50u + 25u*c->retarget_gain;
}
static inline bool gray_config_valid(const gray_config_t *c) {
    if (!c || c->version != 1 || c->retarget_gain > 4 || c->scan_policy > 1) return false;
    for (unsigned a=0; a<4; ++a) for (unsigned b=0; b<4; ++b)
        if (a == b ? c->dose[a][b] != 0 : c->dose[a][b] < 1 || c->dose[a][b] > GRAY_DOSE_MAX) return false;
    return true;
}
