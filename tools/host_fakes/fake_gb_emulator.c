#include "fake_gb_emulator.h"
#include "host_fakes.h"
#include <assert.h>
#include <string.h>

fake_gb_t fake_gb;

void fake_gb_reset(void)
{
    memset(&fake_gb, 0, sizeof(fake_gb));
    fake_gb.init_ok = true;
    fake_gb.state_size = 1;
}

bool gb_emulator_validate_rom(const uint8_t *rom, size_t size) { (void)rom; (void)size; return true; }
size_t gb_emulator_rom_save_size(const uint8_t *rom, size_t size) { (void)rom; fake_gb.last_rom_size = size; return fake_gb.rom_save_size; }

bool gb_emulator_init(const uint8_t *rom, size_t size)
{
    (void)rom;
    ++fake_gb.init_calls;
    fake_gb.last_rom_size = size;
    return fake_gb.init_ok;
}

bool gb_emulator_frame(bool skip_render) { (void)skip_render; return true; }
const uint8_t *gb_emulator_pixels(void) { return fake_gb.pixels; }
void gb_emulator_buttons(uint8_t pressed) { (void)pressed; }
int gb_emulator_error(void) { return 0; }
gb_emulator_diagnostics_t gb_emulator_diagnostics(void) { return (gb_emulator_diagnostics_t){0}; }
void gb_emulator_shutdown(void) {}
size_t gb_emulator_save_size(void) { return fake_gb.save_size; }
bool gb_emulator_save_dirty(void) { return fake_gb.save_dirty; }

bool gb_emulator_save_copy(uint8_t *dst, size_t size)
{
    assert(host_mutexes_held && size == fake_gb.save_size);
    if (fake_gb.save_copy_fails) return false;
    memcpy(dst, fake_gb.live_save, size);
    return true;
}

bool gb_emulator_save_load(const uint8_t *src, size_t size)
{
    assert(host_mutexes_held && size == fake_gb.save_size);
    memcpy(fake_gb.live_save, src, size);
    return true;
}

void gb_emulator_save_clean(void) { fake_gb.save_dirty = false; }
size_t gb_emulator_audio_samples(int16_t *dst, size_t capacity) { (void)dst; (void)capacity; return 0; }
size_t gb_emulator_state_size(void) { return fake_gb.state_size; }

bool gb_emulator_state_copy(uint8_t *dst, size_t size)
{
    assert(host_mutexes_held && size == fake_gb.state_size);
    if (fake_gb.state_copy_fails) return false;
    memcpy(dst, fake_gb.live_state, size);
    return true;
}

bool gb_emulator_state_load(const uint8_t *src, size_t size)
{
    assert(host_mutexes_held && size == fake_gb.state_size);
    if (fake_gb.state_load_fails) return false;
    memcpy(fake_gb.live_state, src, size);
    return true;
}
