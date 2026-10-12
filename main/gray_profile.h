#pragma once
#include "gray_config.h"
#include "gray_adaptive.h"
#include "esp_err.h"
#include "nvs.h"

/* Byte-only, versioned snapshot: no compiler padding or endian dependence. */
typedef struct {
    uint8_t version;
    gray_config_t tuning;
    uint8_t strength, immediate, holds_lo, holds_hi;
    gray_adaptive_t adaptive;
} gray_profile_t;
_Static_assert(sizeof(gray_profile_t) == 38, "Stable grayscale profile layout");
#define GRAY_PROFILE_VERSION 3u
static inline unsigned gray_profile_holds(const gray_profile_t *p) {
    return p->holds_lo | ((unsigned)p->holds_hi << 8);
}
static inline bool gray_profile_valid(const gray_profile_t *p) {
    return p && p->version == GRAY_PROFILE_VERSION && gray_config_valid(&p->tuning) &&
        p->strength <= 3 && p->immediate <= 1 && gray_hold_valid(gray_profile_holds(p)) &&
        gray_adaptive_valid(&p->adaptive);
}
/* Handle-based access leaves the namespace and the commit to the caller. */
esp_err_t gray_profile_nvs_get(nvs_handle_t h, const char *key, gray_profile_t *profile);
esp_err_t gray_profile_nvs_set(nvs_handle_t h, const char *key, const gray_profile_t *profile);
