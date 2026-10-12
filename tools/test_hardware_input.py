#!/usr/bin/env python3
"""Test real GPIO gestures and mapping updates through the real screen context."""
from host_build import run
CODE = r'''
#include <assert.h>
#include <string.h>
#include "input.h"
#include "screen.h"
#include "controller.h"
#include "display_config.h"
#include "driver/gpio.h"
#include "host_fakes.h"
static unsigned fulls, updates, prompts, cancel_step, console_requests;
static bool raw_pending;
static void sample(int pin, int level, int64_t delay) {
    host_gpio_level[pin]=level; host_clock_us+=delay; input_sample_hardware();
}
static void init(void) {
    host_fakes_reset(); input_set_gameplay(false); input_clear_events();
    assert(input_start_hardware_watch()==ESP_OK);
}
static void tap(unsigned pin, unsigned expected) {
    sample(pin,0,1); sample(pin,0,29999); assert(input_hardware_event()==0);
    sample(pin,0,1); sample(pin,1,100000); sample(pin,1,30000);
    assert(input_hardware_event()==expected && !input_hardware_event());
}
static void hold(unsigned pin, unsigned expected) {
    sample(pin,0,1); sample(pin,0,30000); sample(pin,0,699999); assert(!input_hardware_event());
    sample(pin,0,1); assert(input_hardware_event()==expected);
    sample(pin,0,1000000); sample(pin,1,1); sample(pin,1,30000); assert(!input_hardware_event());
}
uint8_t controller_raw(void) {
    if (!raw_pending) return 0;
    raw_pending=false; return (uint8_t)(1u<<(prompts-1));
}
bool controller_connected(void) { return true; }
uint32_t controller_reports(void) { return 0; }
esp_err_t controller_toggle_usb(void) { ++console_requests; return ESP_OK; }
controller_transport_t controller_transport(void) { return CONTROLLER_BLUETOOTH; }
void screen_draw_message(uint8_t *p,const char *title,const char *a,const char *b,const char *c) {
    (void)p;(void)a;(void)b;(void)c;
    if (!strncmp(title,"PRESS ",6)) {
        ++prompts; raw_pending=true;
        if(prompts==cancel_step) { hold(18,INPUT_BACK); hold(18,INPUT_BACK); /* consumed checks; queue one final Back */
            sample(18,0,1); sample(18,0,30000); sample(18,0,700000); }
    }
}
esp_err_t ssd1681_create(ssd1681_handle_t *d) { *d=(ssd1681_handle_t)1; return ESP_OK; }
esp_err_t ssd1681_power_on(ssd1681_handle_t d) { (void)d;return ESP_OK; }
esp_err_t ssd1681_destroy(ssd1681_handle_t d) { (void)d;return ESP_OK; }
esp_err_t ssd1681_analog_power(ssd1681_handle_t d,bool enabled,ssd1681_timing_t *t) { (void)d;(void)enabled;(void)t;return ESP_OK; }
void fast_display_set_gray_game(const uint8_t *p) { (void)p; }
unsigned fast_display_pending(ssd1681_handle_t d) { (void)d;return 0; }
unsigned fast_display_gray_last_scans(void) { return 1; }
void fast_display_wait_slot(ssd1681_handle_t d,int64_t deadline) { (void)d;(void)deadline; }
esp_err_t fast_display_set_dither_pulses(unsigned b,unsigned w) { (void)b;(void)w;return ESP_OK; }
esp_err_t fast_display_full(ssd1681_handle_t d,const uint8_t *p,display_mode_t mode) { (void)d;(void)p;(void)mode; ++fulls; return ESP_OK; }
esp_err_t fast_display_update(ssd1681_handle_t d,const uint8_t *p) { (void)d;(void)p; ++updates; return ESP_OK; }
int main(int argc,char **argv) {
    assert(argc==2); init();
    if(!strcmp(argv[1],"gestures")) {
        tap(0,INPUT_SELECT); tap(18,INPUT_NEXT); hold(0,INPUT_PREVIOUS); hold(18,INPUT_BACK);
        input_set_gameplay(true);
        for(unsigned i=0;i<2;++i) {
            int pin=i?18:0; sample(pin,0,1);sample(pin,0,30000);
            assert(input_hardware_event()==(i?INPUT_NEXT:INPUT_SELECT));
            input_set_gameplay(false);sample(pin,1,10000);sample(pin,1,30000);assert(!input_hardware_event());
            input_set_gameplay(true);
        }
        input_set_gameplay(false);
        sample(0,0,1);sample(18,0,1);sample(0,0,30000);sample(18,0,700000);
        sample(0,1,1);sample(18,1,1);sample(0,1,30000);assert(!input_hardware_event());
        host_gpio_level[0]=0;assert(input_start_hardware_watch()==ESP_OK);
        sample(0,1,1000000);sample(0,1,30000);assert(!input_hardware_event());
    } else if(!strcmp(argv[1],"console")) {
        sample(0,0,1);sample(18,0,1);sample(0,0,30000);
        sample(18,0,2000000);assert(input_hardware_event()==INPUT_PC && console_requests==1);
        sample(18,0,2000000);assert(!input_hardware_event() && console_requests==1);
        sample(0,1,1);sample(18,1,1);sample(0,1,30000);assert(!input_hardware_event());
    } else if(!strcmp(argv[1],"console_continuous")) {
        sample(0,0,1);sample(18,0,30000);sample(18,0,1000000);
        sample(0,1,1);sample(0,1,30000);sample(18,0,2000000);
        assert(!input_hardware_event() && console_requests==0);
        sample(0,0,1);sample(0,0,1999999);assert(!input_hardware_event());
        sample(0,0,1);assert(input_hardware_event()==INPUT_PC && console_requests==1);
    } else {
        nvs_handle_t n;assert(nvs_open("gamepad",NVS_READWRITE,&n)==ESP_OK);input_load_mapping(n);
        uint8_t old[8];size_t size=sizeof old;
        assert(input_mapping_valid());
        if(!strcmp(argv[1],"cancel")) cancel_step=4;
        if(!strcmp(argv[1],"failure")) host_nvs_commit_error=ESP_FAIL;
        bool ok=input_calibrate();
        if(!strcmp(argv[1],"map")) {
            assert(ok && prompts==8 && fulls==1 && updates==7 && host_nvs_commits==1);
            assert(nvs_get_blob(n,"buttons",old,&size)==ESP_OK);
            const uint8_t expected[]={16,32,128,64,8,4,1,2};assert(!memcmp(old,expected,8));
        } else assert(!ok && input_mapping_valid());
    }
    return 0;
}
'''
run(CODE, modules=['input.c','screen.c','display_config.c'], scenarios=[[x] for x in ['gestures','console','console_continuous','map','cancel','failure']])
print('PASS: debounce, two-button short/hold actions, immediate gameplay events, chords/held wake, partial mapping and cancellation/save failure')
