#!/usr/bin/env python3
"""Exercise paused checkpoint flow in the real game session: failures, retry, and restore without resaving."""
from host_build import run, DISPLAY_MODULES, SESSION_FAKES, GB_VIDEO

CODE = r'''
#include "game_session.c"
#include <assert.h>
#include "fake_game_storage.h"
#include "fake_gb_emulator.h"
#include "fake_menu.h"
#include "host_fakes.h"
#include "host_panel.h"

static void fail_battery_save(bool fail)
{
    fake_gb.save_dirty = fail;
    fake_storage.save_error = fail ? ESP_FAIL : ESP_OK;
}

int main(void)
{
    host_fakes_reset();
    host_panel_reset();
    fake_gb_reset();
    fake_storage_reset();
    strcpy(host_task_autorun_name, "game_save");
    assert(game_session_start());
    fake_storage.rom_size = 32768;
    fake_gb.rom_save_size = fake_gb.save_size = 16;
    game_entry_t game = {"test.gb"};
    assert(load_game(&game) && game_ready);

    assert(game_session_ensure_saved() && !fake_storage.state_writes);
    fake_gb.live_state[0] = 17;
    state_dirty = true; host_heap_fail_all = true;
    assert(!game_session_ensure_saved() && state_dirty && !fake_storage.state_writes);
    host_heap_fail_all = false; fake_gb.state_copy_fails = true;
    assert(!game_session_ensure_saved() && state_dirty && !fake_storage.state_writes);
    fake_gb.state_copy_fails = false; fake_storage.save_state_error = ESP_FAIL;
    assert(!game_session_ensure_saved() && state_dirty && fake_storage.state_writes == 1);
    fake_storage.save_state_error = ESP_OK;
    assert(game_session_ensure_saved() && !state_dirty && fake_storage.disk_state[0] == 17 && fake_storage.state_writes == 2);
    assert(game_session_ensure_saved() && fake_storage.state_writes == 2);
    fake_gb.live_state[0] = 91; state_dirty = true; fake_storage.load_state_error = ESP_ERR_NOT_FOUND;
    assert(!game_session_load_checkpoint() && fake_gb.live_state[0] == 91 && state_dirty);
    fake_storage.load_state_error = ESP_OK; fake_gb.state_load_fails = true;
    assert(!game_session_load_checkpoint() && fake_gb.live_state[0] == 91 && state_dirty);
    fake_gb.state_load_fails = false;
    assert(game_session_load_checkpoint() && fake_gb.live_state[0] == 17 && !state_dirty && fake_storage.state_writes == 2);
    assert(game_session_ensure_saved() && fake_storage.state_writes == 2); /* Resume cannot overwrite restored checkpoint. */
    state_dirty = true; fail_battery_save(true);
    assert(!game_session_ensure_saved() && state_dirty && fake_storage.state_writes == 2);
    fail_battery_save(false);
    assert(game_session_ensure_saved() && fake_storage.state_writes == 3);
    game_ready = false; state_dirty = true;
    assert(game_session_ensure_saved() && fake_storage.state_writes == 3);
    assert(!host_mutexes_held && fake_storage.state_reads == 3 && fake_menu_notices == 6);
    return 0;
}
'''

run(CODE, modules=DISPLAY_MODULES, fakes=SESSION_FAKES, extra_sources=[GB_VIDEO])
print('PASS: checkpoint failure preserves dirty state; retry saves; restore never overwrites the checkpoint; SD operations outside core lock')
