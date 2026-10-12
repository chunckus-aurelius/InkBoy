#!/usr/bin/env python3
"""Exercise the real autosave snapshot/retry transaction of the game session."""
from host_build import run, DISPLAY_MODULES, SESSION_FAKES, GB_VIDEO

CODE = r'''
#include "game_session.c"
#include <assert.h>
#include "fake_game_storage.h"
#include "fake_gb_emulator.h"
#include "host_fakes.h"
#include "host_panel.h"

int main(void)
{
    host_fakes_reset();
    host_panel_reset();
    fake_gb_reset();
    fake_storage_reset();
    core_lock = xSemaphoreCreateMutex();
    save_worker_lock = xSemaphoreCreateMutex();
    strcpy(current_game.name, "test.gb");
    save_size = save_worker_size = fake_gb.save_size = 4;
    uint8_t snapshot[4];
    save_worker_buffer = snapshot;
    uint8_t *live = fake_gb.live_save, *disk = fake_storage.disk_save;

    /* req1 wakes the task; req2 arrives before it claims the request. The
     * claim must absorb req2, and its leftover notification must be ignored. */
    unsigned notifications = 1;
    uint32_t claimed = 0;
    save_worker_issued = 1; save_worker_requested = true;
    --notifications; /* ulTaskNotifyTake consumed req1 */
    ++save_worker_issued; save_worker_requested = true; ++notifications; /* req2 */
    assert(save_worker_claim(&claimed) && claimed == 2 && !save_worker_requested);
    save_worker_completed = claimed; save_worker_result = ESP_OK;
    bool success = false;
    assert(save_worker_finished(2, &success) && success);
    assert(notifications == 1);
    --notifications; /* stale coalesced notification */
    assert(!save_worker_claim(&claimed) && save_worker_completed == 2);
    /* A drain can now reconfigure safely; a stale wake cannot access its buffer. */
    uint8_t replacement[4];
    save_worker_buffer = replacement; live[0] = 1; fake_gb.save_dirty = true;
    assert(!save_worker_claim(&claimed) && !fake_storage.save_writes);

    live[0] = 1; fake_gb.save_dirty = true; fake_storage.save_error = ESP_FAIL;
    assert(save_worker_process() == ESP_FAIL && !fake_gb.save_dirty && save_worker_pending && fake_storage.save_writes == 1);
    /* The retained bytes are worker-owned and cannot alias future emulator RAM. */
    memset(live, 9, 4); fake_storage.save_error = ESP_OK;
    assert(save_worker_process() == ESP_OK && !save_worker_pending && fake_storage.save_writes == 2);
    assert(disk[0] == 1 && disk[1] == 0);
    /* New dirty data replaces a failed older snapshot before retrying. */
    live[0] = 4; fake_gb.save_dirty = true; fake_storage.save_error = ESP_FAIL;
    assert(save_worker_process() == ESP_FAIL && save_worker_pending && fake_storage.save_writes == 3);
    live[0] = 7; fake_gb.save_dirty = true; fake_storage.save_error = ESP_OK;
    assert(save_worker_process() == ESP_OK && !save_worker_pending && fake_storage.save_writes == 4 && disk[0] == 7);
    fake_gb.save_dirty = true; fake_gb.save_copy_fails = true;
    assert(save_worker_process() == ESP_FAIL && fake_gb.save_dirty && !save_worker_pending && fake_storage.save_writes == 4);
    fake_gb.save_copy_fails = false;
    assert(save_worker_process() == ESP_OK && !fake_gb.save_dirty && fake_storage.save_writes == 5 && disk[0] == 7);
    save_worker_buffer = NULL;
    assert(save_worker_process() == ESP_ERR_INVALID_STATE && fake_storage.save_writes == 5);

    /* Through the worker task: queued requests coalesce into one write, and
     * persist() waits for the write and reports a failed one. */
    assert(save_worker_start());
    save_worker_configure(heap_caps_malloc(4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT), 4);
    memset(live, 0, 4); live[0] = 5; fake_gb.save_dirty = true;
    unsigned writes = fake_storage.save_writes;
    assert(save_worker_request(false) && save_worker_request(false));
    assert(fake_storage.save_writes == writes);
    host_task_run(save_worker_task);
    assert(fake_storage.save_writes == writes + 1 && disk[0] == 5 && save_worker_completed == save_worker_issued);
    strcpy(host_task_autorun_name, "game_save");
    live[0] = 6; fake_gb.save_dirty = true; fake_storage.save_error = ESP_FAIL;
    assert(!persist() && save_worker_pending);
    fake_storage.save_error = ESP_OK;
    assert(persist() && disk[0] == 6 && !save_worker_pending);
    return 0;
}
'''

run(CODE, modules=DISPLAY_MODULES, fakes=SESSION_FAKES, extra_sources=[GB_VIDEO])
print('PASS: notification race/drain, immutable snapshot, retries, newer dirty RAM, copy failure, request coalescing and persist() draining')
