#!/usr/bin/env python3
"""Check game switching in the real game session preserves the old game and save on failures."""
from host_build import run, DISPLAY_MODULES, SESSION_FAKES, GB_VIDEO

CODE = r'''
#include "game_session.c"
#include <assert.h>
#include "fake_game_storage.h"
#include "fake_gb_emulator.h"
#include "fake_menu.h"
#include "host_fakes.h"
#include "host_panel.h"

static bool fail_candidate_buffers(size_t size, unsigned caps)
{
    (void)caps;
    return size == 8192;
}

static void start_with_first_game(void)
{
    host_fakes_reset();
    host_panel_reset();
    fake_gb_reset();
    fake_storage_reset();
    strcpy(host_task_autorun_name, "game_save");
    assert(game_session_start());
    fake_storage.rom_size = 16;
    fake_gb.rom_save_size = fake_gb.save_size = 16;
    game_entry_t first = {"first.gb"};
    assert(load_game(&first) && game_ready && save_size == 16);
    fake_storage.rom_size = 32768;
    fake_gb.rom_save_size = fake_gb.save_size = 8192;
    fake_gb.init_calls = 0;
    fake_storage.save_fill = 90;
    fake_menu_notices = 0;
}

static void blocked_switch(unsigned scenario)
{
    start_with_first_game();
    state_dirty = true;
    fake_gb.live_state[0] = 17;
    uint8_t *old_rom = current_rom;
    size_t old_size = current_rom_size;
    uint32_t old_crc = current_rom_crc;
    uint8_t *old_buffer = save_worker_buffer;
    if (scenario == 1) fake_storage.save_state_error = ESP_FAIL; /* Unsaved progress cannot be written. */
    if (scenario == 2) fake_storage.read_error = ESP_ERR_NOT_SUPPORTED; /* Invalid ROM. */
    if (scenario == 3) fake_storage.load_save_error = ESP_ERR_INVALID_CRC; /* Corrupt existing save. */
    if (scenario == 4) host_heap_fail_hook = fail_candidate_buffers;
    game_entry_t second = {"second.gb"};
    assert(!load_game(&second) && !fake_gb.init_calls && game_ready && save_size == 16);
    assert(fake_menu_notices == 1);
    assert(current_rom_size == old_size && current_rom_crc == old_crc);
    assert(current_rom == old_rom && !strcmp(current_game.name, "first.gb"));
    assert(save_worker_buffer == old_buffer && save_worker_size == 16); /* The previous game's writer is untouched. */
    assert(state_dirty == (scenario == 1)); /* Progress is checkpointed first, or stays dirty if that failed. */
}

static void switch_game(unsigned scenario)
{
    start_with_first_game();
    state_dirty = true;
    if (scenario == 5) fake_storage.load_save_error = ESP_ERR_NOT_FOUND; /* Game without a save yet. */
    if (scenario == 6) fake_gb.init_ok = false; /* Core refuses the cartridge. */
    game_entry_t second = {"second.gb"};
    bool ok = load_game(&second);
    assert(fake_gb.init_calls == 1 && save_size == 8192 && save_worker_size == 8192 && save_worker_buffer);
    assert(!strcmp(current_game.name, "second.gb"));
    assert(current_rom_size == 32768 && current_rom_crc == game_storage_crc32(current_rom, current_rom_size));
    assert(!state_dirty);
    assert(ok == (scenario != 6) && game_ready == ok);
    if (scenario == 0 || scenario == 5) assert(fake_gb.live_save[0] == (scenario == 0 ? 90 : 0));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    unsigned scenario = (unsigned)(argv[1][0] - '0');
    if (scenario >= 1 && scenario <= 4) blocked_switch(scenario);
    else switch_game(scenario);
    return 0;
}
'''

run(CODE, modules=DISPLAY_MODULES, fakes=SESSION_FAKES, extra_sources=[GB_VIDEO],
    scenarios=[[str(n)] for n in range(7)])
print('PASS: switch blocked on unsaved progress, invalid ROM, corrupt save or allocation failure; per-game load, new save and core failure')
