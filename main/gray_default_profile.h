#pragma once
#include "gray_profile.h"

static inline const gray_profile_t *gray_default_profile(void)
{
    static const gray_profile_t profile={
        .version=GRAY_PROFILE_VERSION,
        .tuning=GRAY_CONFIG_DEFAULT,
        .strength=GRAY_STRENGTH_DEFAULT, .immediate=GRAY_IMMEDIATE_DEFAULT,
        .holds_lo=GRAY_HOLDS_DEFAULT & 0xffu, .holds_hi=GRAY_HOLDS_DEFAULT >> 8,
        .adaptive=GRAY_ADAPTIVE_DEFAULT,
    };
    return &profile;
}
