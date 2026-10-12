#pragma once
#include <stdbool.h>

bool game_session_start(void);

bool game_session_ready(void);
const char *game_session_name(void);

bool game_session_select(void);
unsigned game_session_play(void);

bool game_session_ensure_saved(void);
void game_session_save_state(void);
bool game_session_load_checkpoint(void);
