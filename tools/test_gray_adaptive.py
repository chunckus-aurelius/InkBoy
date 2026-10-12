#!/usr/bin/env python3
"""Execute production history/selective masks against acknowledged delivery fakes."""
from host_build import run

CODE = r'''
#include "fast_display.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "gray_plan.h"
#include "host_fakes.h"
#include "host_panel.h"

static gray_plan_t plan;
#define TEST_ADAPTIVE {0,0,{-1,0,0,0,0,0},{150,0,0,0,0,0}}
static gray_history_t history[GRAY_PIXELS];
static uint8_t binary[5000],game[5760],bm[5000],wm[5000];
static const unsigned probe=28*200+20;
static void target(unsigned shade){memset(game,(3-shade)*85,5760);gray_plan_target(&plan,binary,game);}
static void pulse(void){gray_plan_masks(&plan,bm,wm);gray_plan_complete(&plan,bm,wm);}
static void drain(void){while(plan.pending)pulse();}
static void backend_drain(ssd1681_handle_t d){while(fast_display_pending(d))assert(!fast_display_update(d,binary));}
int main(void){
 host_fakes_reset();host_panel_reset();host_panel.require_gray_frame_rate=true;
 {gray_adaptive_t quiet=TEST_ADAPTIVE;assert(!fast_display_set_gray_adaptive(&quiet));}
 const gray_config_t config={1,{{0,2,4,7},{4,0,1,6},{4,1,0,5},{7,3,2,0}},2,0};
 const unsigned origins[]={2,1,1,2,1,2},vias[]={0,0,3,3,2,1};
 memset(binary,255,sizeof(binary));plan.config=config;plan.history=history;
 plan.adaptive_configured=true;plan.adaptive=(gray_adaptive_t)TEST_ADAPTIVE;
 plan.history_mode=3;
 for(unsigned route=0;route<GRAY_ROUTES;++route){
  // Neutral routes keep the user's calibrated table; L-K-L keeps its -1/150 defaults.
  gray_plan_seed(&plan,binary);target(origins[route]);drain();target(vias[route]);drain();
  unsigned expected=config.dose[vias[route]][origins[route]]-(route==0);
  target(origins[route]);assert(plan.pixel[probe].remaining==expected);
  assert(plan.history_returns==(route==0 ? 23040u : 0u));drain();
 }
 for(unsigned route=0;route<GRAY_ROUTES;++route){
  plan.adaptive.return_delta[route]=-1;plan.adaptive.reverse_percent[route]=150;
  if(route>=4) {plan.config.dose[1][2]=4;plan.config.dose[2][1]=4;}
  gray_plan_seed(&plan,binary);target(origins[route]);drain();target(vias[route]);
  // A prepared but unacknowledged mask is not an excursion.
  gray_plan_masks(&plan,bm,wm);target(origins[route]);assert(!plan.history_reversals);drain();
  target(vias[route]);pulse();assert(plan.pixel[probe].remaining);
  assert(vias[route]<origins[route] ? history[probe].black_steps==1 : history[probe].white_steps==1);
  target(origins[route]);assert(plan.pixel[probe].remaining==2 && plan.history_reversals==23040);drain();
  target(vias[route]);drain();target(origins[route]);
  assert(plan.pixel[probe].remaining==plan.config.dose[vias[route]][origins[route]]-1);drain();
 }
 // Invalid policy is rejected; source provenance is separate from pixel model.
 gray_adaptive_t options=TEST_ADAPTIVE;options.history_mode=3;options.weak_wl=1;
 host_heap_fail_all=true;assert(fast_display_reserve_gray_adaptive(&options)==ESP_ERR_NO_MEM);
 assert(!gray_adaptive.history_mode && !gray_adaptive.weak_wl);host_heap_fail_all=false;
 assert(!fast_display_reserve_gray_adaptive(&options));
 assert(!gray_adaptive.history_mode && !gray_adaptive.weak_wl);
 assert(!fast_display_set_gray_adaptive(&options));assert(gray_history_store && gray_weak_buffers);
 assert(!fast_display_set_gray_config(&config));assert(!fast_display_set_gray_immediate(1));
 assert(!fast_display_set_gray_holds(GRAY_HOLD_ALL));
 ssd1681_handle_t d=(ssd1681_handle_t)1;
 memset(game,0,5760);fast_display_set_gray_game(game);assert(!fast_display_full(d,binary,DISPLAY_GRAYSCALE));
 assert(gray_plan->history && gray_plan->history_mode==3 && gray_plan->adaptive_configured);
 unsigned scans=host_panel.scans;host_panel.special_pixels=0;
 // Only one settled W->L pixel. Other W->D and W->K requests keep strong black.
 game[0]=1u | (2u<<2) | (3u<<4);assert(!fast_display_update(d,binary));
 assert(host_panel.special_pixels==1 && host_panel.scans==scans+2);
 backend_drain(d);assert(host_panel.special_pixels==2);
 assert(gray_plan->pixel[probe].level==128 && gray_plan->pixel[probe+1].level==64 && gray_plan->pixel[probe+2].level==0);
 // L->D remains normal, despite having a gray destination.
 unsigned special_before=host_panel.special_pixels;game[0]=(game[0]&~3u)|2u;
 assert(!fast_display_update(d,binary));backend_drain(d);assert(host_panel.special_pixels==special_before);
 // D->W near completion may round to model source W, but is NOT settled W.
 game[0]&=~3u;
 for(unsigned i=0;i<5;++i)assert(!fast_display_update(d,binary));
 assert(gray_plan->pixel[probe].remaining==1 && gray_plan->pixel[probe].level>=160);
 game[0]|=1u;assert(!fast_display_update(d,binary));backend_drain(d);
 assert(host_panel.special_pixels==special_before);
 // A completed white preparation does qualify, including under 8.8V.
 options.weak_wl=2;unsigned updates_before=host_panel.updates;
 assert(!fast_display_set_gray_adaptive(&options) && host_panel.updates==updates_before);
 game[0]&=~3u;assert(!fast_display_update(d,binary));backend_drain(d);
 game[0]|=1u;assert(!fast_display_update(d,binary));backend_drain(d);
 assert(host_panel.special_pixels==special_before+2 && host_panel.wave_mode==SSD1681_GRAY_WAVE_WEAK88);
 // Reserving the disabled policy must preserve the currently attached policy
 // and its history allocation until a later explicit setter commits OFF.
 gray_adaptive_t disabled=options;disabled.history_mode=0;disabled.weak_wl=0;
 gray_history_t *saved_history=gray_plan->history;uint8_t *saved_buffers=gray_weak_buffers;
 assert(!fast_display_reserve_gray_adaptive(&disabled));
 assert(gray_adaptive.history_mode==3 && gray_plan->history_mode==3 &&
        gray_plan->history==saved_history && gray_weak_buffers==saved_buffers);
 options.history_mode=0;options.weak_wl=0;updates_before=host_panel.updates;
 assert(!fast_display_set_gray_adaptive(&options) && host_panel.updates==updates_before);
 assert(!gray_plan->history && !gray_history_store && !gray_weak_buffers);
 assert(!fast_display_full(d,binary,DISPLAY_DITHERED));
 heap_caps_free(drive_counters);drive_counters=NULL;
 puts("PASS: neutral defaults, all six completed/interrupted routes, delivered-only history, selective W-L eligibility, paired delivery, allocation rejection, live policy and restore");
}
'''

run(CODE, modules=['gray_plan.c', 'gray_plan_parallel.c', 'drive_counters.c'], fakes=['host_panel.c'])
