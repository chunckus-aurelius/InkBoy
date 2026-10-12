#include "gray_settings_ui.h"
#include "display_config.h"
#include "display_preset.h"
#include "fast_display.h"
#include "gb_emulator.h"
#include "gb_video.h"
#include "gray_adaptive.h"
#include "gray_config.h"
#include "gray_motion.h"
#include "gray_profile.h"
#include "input.h"
#include "menu.h"
#include "screen.h"
#include "screen_draw.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "player";
static gray_config_t gray_config = GRAY_CONFIG_DEFAULT;
static gray_adaptive_t gray_adaptive = GRAY_ADAPTIVE_DEFAULT;
static uint8_t gray_strength=GRAY_STRENGTH_DEFAULT;
static bool gray_immediate=GRAY_IMMEDIATE_DEFAULT;
static uint16_t gray_holds=GRAY_HOLDS_DEFAULT;

static gray_profile_t gray_profile_current(void)
{
    return (gray_profile_t){.version=GRAY_PROFILE_VERSION,.tuning=gray_config,.strength=gray_strength,
        .immediate=gray_immediate,.holds_lo=gray_holds & 255,.holds_hi=gray_holds >> 8,.adaptive=gray_adaptive};
}

static bool gray_profile_apply(const gray_profile_t *p)
{
    esp_err_t error=fast_display_set_gray_adaptive(&p->adaptive);
    if(error) {
        ESP_LOGW(TAG,"grayscale policy allocation failed: %s; keeping current profile",esp_err_to_name(error));
        return false;
    }
    gray_adaptive=p->adaptive;
    gray_config=p->tuning;
    gray_strength=p->strength;
    gray_immediate=p->immediate;
    gray_holds=gray_profile_holds(p);
    (void)fast_display_set_gray_config(&gray_config);
    (void)fast_display_set_gray_strength(gray_strength);
    (void)fast_display_set_gray_immediate(gray_immediate);
    (void)fast_display_set_gray_holds(gray_holds);
    ESP_LOGI(TAG,"PLAYER_GRAY_PROFILE,strength,%u,immediate,%u,holds,%u,retarget,%u,scans,%u,history,%u,weak_wl,%u,return_lkl,%d,return_dkd,%d,return_dwd,%d,return_lwl,%d,reverse_lkl,%u,reverse_dkd,%u,reverse_dwd,%u,reverse_lwl,%u,return_dld,%d,return_ldl,%d,reverse_dld,%u,reverse_ldl,%u",
             gray_strength,gray_immediate,gray_holds,gray_retarget_percent(&gray_config),gray_config.scan_policy,
             gray_adaptive.history_mode,gray_adaptive.weak_wl,
             gray_adaptive.return_delta[0],gray_adaptive.return_delta[1],gray_adaptive.return_delta[2],gray_adaptive.return_delta[3],
             gray_adaptive.reverse_percent[0],gray_adaptive.reverse_percent[1],gray_adaptive.reverse_percent[2],gray_adaptive.reverse_percent[3],
             gray_adaptive.return_delta[4],gray_adaptive.return_delta[5],
             gray_adaptive.reverse_percent[4],gray_adaptive.reverse_percent[5]);
    for (unsigned from=0; from<4; ++from)
        ESP_LOGI(TAG,"PLAYER_GRAY_DOSES,from,%u,K,%u,D,%u,L,%u,W,%u",from,
                 gray_config.dose[from][0],gray_config.dose[from][1],
                 gray_config.dose[from][2],gray_config.dose[from][3]);
    return true;
}

/* Reserve optional RAM before committing a new active preset. The backend
 * setter sends no panel I/O. Roll it back on storage failure. */
esp_err_t gray_settings_ui_activate(const display_preset_t *preset, bool modified)
{
    if(!preset || preset->mode!=DISPLAY_GRAYSCALE) return ESP_ERR_INVALID_ARG;
    const gray_profile_t *p=&preset->gray;
    bool changed=memcmp(&p->adaptive,&gray_adaptive,sizeof(gray_adaptive))!=0;
    esp_err_t error=changed ? fast_display_reserve_gray_adaptive(&p->adaptive) : ESP_OK;
    if(error) {
        if(changed) (void)fast_display_set_gray_adaptive(&gray_adaptive);
        return error;
    }
    error=display_preset_set_active(preset,modified);
    if(error) {
        if(changed) (void)fast_display_set_gray_adaptive(&gray_adaptive);
        return error;
    }
    return gray_profile_apply(p) ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t gray_edit_commit(const gray_profile_t *p)
{
    display_preset_t next=*display_preset_active(DISPLAY_GRAYSCALE);
    next.gray=*p;
    return display_preset_set_active(&next,true);
}

static bool gray_strength_save(unsigned value)
{
    if (value>3) return false;
    gray_profile_t p=gray_profile_current();
    p.strength=value;
    if (gray_edit_commit(&p)) {
        menu_notice("SETTINGS ERROR","COULD NOT SAVE STRENGTH","CURRENT VALUE STILL ACTIVE");
        return false;
    }
    gray_strength=value;
    (void)fast_display_set_gray_strength(value);
    ESP_LOGI(TAG,"PLAYER_GRAY_STRENGTH_SAVED,preset,%u",value);
    return true;
}

static bool gray_immediate_save(bool enabled)
{
    gray_profile_t p=gray_profile_current();
    p.immediate=enabled;
    if (gray_edit_commit(&p)) {
        menu_notice("SETTINGS ERROR","COULD NOT SAVE HOLD TIMING","CURRENT VALUE STILL ACTIVE");
        return false;
    }
    gray_immediate=enabled;
    (void)fast_display_set_gray_immediate(enabled);
    ESP_LOGI(TAG,"PLAYER_GRAY_IMMEDIATE,enabled,%u",enabled);
    return true;
}

static bool gray_holds_save(unsigned value)
{
    if (!gray_hold_valid(value)) return false;
    gray_profile_t p=gray_profile_current();
    p.holds_lo=value & 255; p.holds_hi=value >> 8;
    if (gray_edit_commit(&p)) {
        menu_notice("SETTINGS ERROR","COULD NOT SAVE PAIR HOLDS","CURRENT VALUES STILL ACTIVE");
        return false;
    }
    gray_holds=value;
    (void)fast_display_set_gray_holds(value);
    ESP_LOGI(TAG,"PLAYER_GRAY_PAIR_HOLDS,mask,%u",value);
    return true;
}

static bool gray_settings_save(const gray_config_t *candidate)
{
    if (!gray_config_valid(candidate)) return false;
    gray_profile_t p=gray_profile_current();
    p.tuning=*candidate;
    if (gray_edit_commit(&p)) {
        menu_notice("SETTINGS ERROR", "COULD NOT SAVE GRAYSCALE SETTINGS", "CURRENT VALUES STILL ACTIVE");
        return false;
    }
    gray_config = *candidate;
    (void)fast_display_set_gray_config(&gray_config);
    ESP_LOGI(TAG, "PLAYER_GRAY_SETTINGS,retarget_percent,%u,scan_policy,%u",
             gray_retarget_percent(&gray_config), gray_config.scan_policy);
    for (unsigned from=0; from<4; ++from)
        ESP_LOGI(TAG, "PLAYER_GRAY_DOSES,from,%u,K,%u,D,%u,L,%u,W,%u", from,
                 gray_config.dose[from][0], gray_config.dose[from][1],
                 gray_config.dose[from][2], gray_config.dose[from][3]);
    return true;
}

static void gray_dose_settings(bool lighten)
{
    static const uint8_t light_pairs[6][2] = {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}};
    static const uint8_t dark_pairs[6][2] = {{3,2},{3,1},{3,0},{2,1},{2,0},{1,0}};
    static const char *names[] = {"BLACK", "DARK", "LIGHT", "WHITE"};
    const uint8_t (*pairs)[2] = lighten ? light_pairs : dark_pairs;
    unsigned selected = 0;
    while (!screen_failed()) {
        char labels[12][32]; const char *items[13];
        for (unsigned i=0; i<6; ++i) {
            unsigned from=pairs[i][0], to=pairs[i][1];
            snprintf(labels[2*i], sizeof(labels[0]), "%s > %s: %u", names[from], names[to], gray_config.dose[from][to]);
            snprintf(labels[2*i+1],sizeof(labels[0]),"%s > %s HOLD: %s",names[from],names[to],
                     gray_holds & (1u << (from*4+to)) ? "ON" : "OFF");
            items[2*i] = labels[2*i]; items[2*i+1] = labels[2*i+1];
        }
        items[12] = MENU_BACK;
        int direction=1;
        int choice=menu_choose(lighten ? "LIGHTEN DOSES" : "DARKEN DOSES",
                        gray_immediate ? "LEFT/RIGHT: DOSE OR HOLD" : "PAIR MODE OFF - HOLDS INACTIVE",
                        items, 13, &selected, &direction);
        if (choice<0 || choice==12) return;
        unsigned pair=(unsigned)choice/2;
        if (choice & 1) {
            (void)gray_holds_save(gray_holds ^ (1u << (pairs[pair][0]*4+pairs[pair][1])));
            continue;
        }
        gray_config_t next=gray_config;
        uint8_t *dose=&next.dose[pairs[pair][0]][pairs[pair][1]];
        if (direction>0 && *dose<GRAY_DOSE_MAX) ++*dose;
        if (direction<0 && *dose>1) --*dose;
        if (memcmp(&next, &gray_config, sizeof(next))) (void)gray_settings_save(&next);
    }
}

/* Every entry starts a fresh display context and full reference.
 * No ROM or emulator work: the paused game/save state is left untouched. */
void gray_settings_ui_transition_test(void)
{
    uint8_t *const panel=screen_panel(), *const packed=screen_packed_image();
    gb_video_t *const video=screen_video();
    if (!screen_close()) return;
    const UBaseType_t priority=uxTaskPriorityGet(NULL);
    screen_set_gray_source(packed);
    vTaskPrioritySet(NULL,20);
    input_wait_release(); input_clear_events();
    uint8_t held=input_buttons();
    bool stopped=false, old_stopped=false;
    unsigned state=0, old_state=0, reason=0;
    int64_t elapsed=0, next_image=0;
    memset(video,0,sizeof(*video));
    screen_draw_gray_motion_labels(panel,0,false);
    gray_motion_render(packed,0);
    (void)gb_video_prepare_gray(video,packed,panel);
    bool ok=screen_present(true);
    int64_t last_tick=esp_timer_get_time();
    ESP_LOGI(TAG,"PLAYER_GRAY_TEST_BEGIN,mode,%u,travel_us,%lld,retarget_percent,%u,scan_policy,%u",
             display_config_get().mode,(long long)GRAY_MOTION_TRAVEL_US,gray_retarget_percent(&gray_config),gray_config.scan_policy);
    while (ok && !screen_failed()) {
        if (screen_lifetime_reached()) { reason=1; break; }
        screen_wait_slot(next_image);
        int64_t now=esp_timer_get_time();
        if (!stopped) elapsed += now-last_tick;
        last_tick=now;
        unsigned boot=input_hardware_event();
        uint8_t raw=input_buttons(), pressed=raw & (uint8_t)~held;
        held=raw;
        if (boot>=INPUT_BACK || (pressed & GB_BTN_B)) break;
        if (boot==INPUT_SELECT || (pressed & GB_BTN_A)) stopped=!stopped;
        if (boot==INPUT_NEXT || (pressed & GB_BTN_START)) { elapsed=0; next_image=0; }
        if (pressed & GB_BTN_SELECT) {
            /* Explicit manual cleaning restarts the sweep from its known seed. */
            elapsed=0; next_image=0;
            screen_draw_gray_motion_labels(panel,0,stopped);
            gray_motion_render(packed,0);
            (void)gb_video_prepare_gray(video,packed,panel);
            fast_display_set_gray_game(packed);
            esp_err_t error=screen_clean();
            if (error) { screen_fail(); ok=false; break; }
            old_state=0; old_stopped=stopped;
            last_tick=esp_timer_get_time();
            ESP_LOGI(TAG,"PLAYER_GRAY_TEST_CLEAN,updates,%u",screen_updates());
            continue;
        }
        bool changed=false;
        if (now>=next_image) {
            state=gray_motion_state(elapsed);
            unsigned x=gray_motion_x(elapsed);
            bool labels=state!=old_state || stopped!=old_stopped;
            if (labels) screen_draw_gray_motion_labels(panel,state,stopped);
            gray_motion_render(packed,x);
            changed=gb_video_prepare_gray(video,packed,panel)!=0 || labels;
            old_state=state; old_stopped=stopped;
            ESP_LOGI(TAG,"PLAYER_GRAY_TEST_FRAME,time_us,%" PRId64 ",elapsed_us,%" PRId64 ",x,%u,state,%u,paused,%u",
                     now,elapsed,x,state,stopped);
            next_image=fast_display_next_image(next_image,now);
        }
        if (changed || screen_pending()) ok=screen_present(changed);
    }
    fast_display_set_gray_game(NULL);
    screen_set_gray_source(NULL);
    vTaskPrioritySet(NULL,priority);
    if (!ok || screen_failed()) (void)screen_close();
    ESP_LOGI(TAG,"PLAYER_GRAY_TEST_END,reason,%u,error,%u",reason,!ok || screen_failed());
}

/* These settings follow presets into games and the transition test; adjusting them does not clean the panel. */
static bool gray_adaptive_save(const gray_adaptive_t *next)
{
    if(!gray_adaptive_valid(next)) return false;
    display_preset_t preset=*display_preset_active(DISPLAY_GRAYSCALE);
    preset.gray=gray_profile_current();
    preset.gray.adaptive=*next;
    esp_err_t error=gray_settings_ui_activate(&preset,true);
    if(error) menu_notice("SETTINGS ERROR","COULD NOT APPLY HISTORY","CURRENT SETTINGS KEPT");
    return error==ESP_OK;
}

static void gray_adaptive_route_settings(unsigned route)
{
    static const char *names[]={"LIGHT > BLACK > LIGHT","DARK > BLACK > DARK","DARK > WHITE > DARK","LIGHT > WHITE > LIGHT",
        "DARK > LIGHT > DARK","LIGHT > DARK > LIGHT"};
    unsigned selected=0;
    while(!screen_failed()) {
        char done[32],reverse[32];
        snprintf(done,sizeof(done),"COMPLETED DOSE: %+d",gray_adaptive.return_delta[route]);
        unsigned gain=gray_adaptive.reverse_percent[route];
        if(gain) snprintf(reverse,sizeof(reverse),"REVERSE DOSE: %u%%",gain);
        else snprintf(reverse,sizeof(reverse),"REVERSE DOSE: BASE");
        const char *items[]={done,reverse,"RUN TRANSITION TEST",MENU_BACK};
        int direction=1;
        int choice=menu_choose(names[route],"REQUIRES HISTORY MODE",items,4,&selected,&direction);
        if(choice<0 || choice==3) return;
        if(choice==2) { gray_settings_ui_transition_test();continue; }
        gray_adaptive_t next=gray_adaptive;
        if(choice==0) {
            int value=next.return_delta[route]+(direction>0 ? 1 : -1);
            if(value>=-3 && value<=3) next.return_delta[route]=value;
        } else {
            int value=gain ? gain/25-1 : 0; /* BASE,50,75,...,200 */
            value+=direction>0 ? 1 : -1;
            if(value>=0 && value<=7) next.reverse_percent[route]=value ? (value+1)*25 : 0;
        }
        if(memcmp(&next,&gray_adaptive,sizeof(next))) (void)gray_adaptive_save(&next);
    }
}

static void gray_adaptive_settings(void)
{
    static const char *modes[]={"OFF","RETURNS","REVERSALS","BOTH"};
    static const char *strengths[]={"NORMAL","11V","8.8V"};
    unsigned selected=0;
    while(!screen_failed()) {
        char history[32],weak[32];
        snprintf(history,sizeof(history),"HISTORY: %s",modes[gray_adaptive.history_mode]);
        snprintf(weak,sizeof(weak),"SETTLED W>L: %s",strengths[gray_adaptive.weak_wl]);
        const char *items[]={history,weak,"L > K > L CONTROLS","D > K > D CONTROLS",
            "D > W > D CONTROLS","L > W > L CONTROLS","D > L > D CONTROLS","L > D > L CONTROLS",
            "RUN TRANSITION TEST","RESET THESE OPTIONS",MENU_BACK};
        int direction=1;
        int choice=menu_choose("GRAYSCALE HISTORY / DRIVE","LIVE / SAVED / NO CLEAN",items,11,&selected,&direction);
        if(choice<0 || choice==10) return;
        if(choice>=2 && choice<=7) { gray_adaptive_route_settings(choice-2);continue; }
        if(choice==8) { gray_settings_ui_transition_test();continue; }
        gray_adaptive_t next=gray_adaptive;
        if(choice==0) next.history_mode=(next.history_mode+(direction>0 ? 1 : 3))%4;
        else if(choice==1) next.weak_wl=(next.weak_wl+(direction>0 ? 1 : 2))%3;
        else next=(gray_adaptive_t)GRAY_ADAPTIVE_DEFAULT;
        if(memcmp(&next,&gray_adaptive,sizeof(next))) (void)gray_adaptive_save(&next);
    }
}

void gray_settings_ui_open(void)
{
    static const char *strengths[]={"STOCK -15V","WEAKER -13V","WEAKER -11V","WEAKER -9V"};
    unsigned selected=0;
    while (!screen_failed()) {
        char gain[32], scans[32], strength[40], pair_mode[32];
        snprintf(gain, sizeof(gain), "RETARGET DOSE: %u%%", gray_retarget_percent(&gray_config));
        snprintf(scans, sizeof(scans), "SCANS/FRAME: %s", gray_immediate ? "PAIR" : gray_config.scan_policy ? "1" : "AUTO 1-2");
        snprintf(strength, sizeof(strength), "LIGHTEN: %s", strengths[gray_strength]);
        snprintf(pair_mode, sizeof(pair_mode), "PAIR HOLD MODE: %s", gray_immediate ? "ON" : "OFF");
        const char *items[] = {"LIGHTEN DOSES", "DARKEN DOSES", gain, scans, "RUN TRANSITION TEST", strength, pair_mode, "HISTORY / SELECTIVE", MENU_BACK};
        int direction=1;
        int choice=menu_choose("GRAYSCALE TUNING", "LIVE / SAVED / NO CLEAN", items, 9, &selected, &direction);
        if (choice<0 || choice==8) return;
        if (choice==7) { gray_adaptive_settings(); continue; }
        if (choice==6) { (void)gray_immediate_save(!gray_immediate); continue; }
        if (choice==5) {
            unsigned value=gray_strength;
            if (direction>0 && value<3) ++value;
            else if (direction<0 && value>0) --value;
            if (value!=gray_strength) (void)gray_strength_save(value);
            continue;
        }
        if (choice==4) { gray_settings_ui_transition_test(); continue; }
        if (choice<2) { gray_dose_settings(choice==0); continue; }
        gray_config_t next=gray_config;
        if (choice==2) {
            if (direction>0 && next.retarget_gain<4) ++next.retarget_gain;
            if (direction<0 && next.retarget_gain>0) --next.retarget_gain;
        } else {
            if (gray_immediate) continue; /* Immediate pairs occupy one image slot. */
            next.scan_policy ^= 1;
        }
        if (memcmp(&next, &gray_config, sizeof(next))) (void)gray_settings_save(&next);
    }
}

void gray_settings_ui_load(void)
{
    (void)gray_profile_apply(&display_preset_active(DISPLAY_GRAYSCALE)->gray);
}
