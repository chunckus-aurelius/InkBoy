#pragma once
#include "gb_emulator.h"

/* Emulator stand-in. Frame, save-RAM and state buffers are tiny byte arrays;
 * the calls assert the contract that they run under the player's core lock. */
typedef struct {
    uint8_t pixels[GB_PACKED_BYTES];
    uint8_t live_save[8192], live_state[16];
    size_t rom_save_size, save_size, state_size, last_rom_size;
    bool init_ok, save_dirty, save_copy_fails, state_copy_fails, state_load_fails;
    unsigned init_calls;
} fake_gb_t;

extern fake_gb_t fake_gb;
void fake_gb_reset(void);
