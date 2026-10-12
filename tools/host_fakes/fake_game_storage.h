#pragma once
#include "game_storage.h"

/* In-memory card: one ROM, one battery save and one checkpoint. Card I/O must
 * never run while the emulator core lock is held. */
typedef struct {
    char root[256];
    esp_err_t mount_error;
    esp_err_t read_error, load_save_error, save_error, save_state_error, load_state_error,
              has_state_error, list_error, close_error;
    size_t rom_size, list_count;
    uint8_t save_fill;
    uint8_t disk_save[8192], disk_state[16];
    size_t disk_save_size;
    bool has_state;
    game_entry_t listed[GAME_CATALOG_MAX];
    unsigned save_writes, state_writes, state_reads;
} fake_storage_t;

extern fake_storage_t fake_storage;
void fake_storage_reset(void);
