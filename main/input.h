#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

uint8_t input_buttons(void);

enum { INPUT_NONE, INPUT_NEXT, INPUT_SELECT, INPUT_PREVIOUS, INPUT_BACK, INPUT_PC };
unsigned input_hardware_event(void);
void input_clear_events(void);
void input_set_gameplay(bool enabled);
void input_sample_hardware(void);
esp_err_t input_start_hardware_watch(void);

void input_load_mapping(nvs_handle_t settings);
bool input_mapping_valid(void);
bool input_calibrate(void);
void input_remap(void);

bool input_connected(void);
uint32_t input_report_count(void);
bool input_wait_controller(void);
void input_wait_release(void);
