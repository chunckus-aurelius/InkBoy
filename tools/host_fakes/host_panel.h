#pragma once
/* Fake SSD1681 driver with the state rules of the real one: the order and
 * preconditions of the vendor calls are asserted, scans advance the virtual
 * clock, and a test can inject failures or observe each delivered scan. */
#include "ssd1681_fast.h"

/* Timing of the first HOST_PANEL_SCANS scans is recorded; the counters cover all. */
#define HOST_PANEL_SCANS 8192
#define HOST_PANEL_ERROR 99

typedef struct {
    bool powered, analog, prepared, explicit_masks, fast_transport, continuous_play;
    unsigned frame_rate, strength, wave_mode, mixed_frames;
    unsigned continuous_enables, updates, fulls, scans, resumes, analog_offs, destroys, strength_changes, special_pixels;
    uint8_t physical[SSD1681_FRAME_BYTES];
    int64_t last_scan_start;
    int64_t scan_start[HOST_PANEL_SCANS], scan_end[HOST_PANEL_SCANS];
    bool scan_hold[HOST_PANEL_SCANS];
    bool any_hold; /* Some scan, recorded or not, drove no pixel. */

    int64_t scan_us, first_scan_extra_us, resume_us, full_us;
    unsigned fail_scan_at;
    bool scan_fails;
    bool require_gray_frame_rate;
    bool strict_power_off; /* Powering analog off when it is already off is a bug. */
    unsigned min_gap_fast_us, min_gap_slow_us;
    /* The full-update cap is enforced unless continuous play was requested. */
    void (*on_full)(const uint8_t *frame);
    void (*before_scan)(const uint8_t *black, const uint8_t *white);
    void (*after_scan)(esp_err_t error);
} host_panel_t;

extern host_panel_t host_panel;
void host_panel_reset(void);
