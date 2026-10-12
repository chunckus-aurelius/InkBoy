#!/usr/bin/env python3
"""Check the actual minimal framebuffer and optional exported previews."""
from host_build import run
CODE=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "screen_draw.h"
static uint8_t first[5000], second[5000];
static void save(const char *name,const uint8_t *frame) {
    const char *folder=getenv("INKBOY_UI_IMAGES");if(!folder)return;
    char path[512];snprintf(path,sizeof path,"%s/%s.pgm",folder,name);FILE *f=fopen(path,"w");assert(f);
    fprintf(f,"P2\n200 200\n255\n");
    for(unsigned y=0;y<200;++y)for(unsigned x=0;x<200;++x)fprintf(f,"%u ",frame[y*25+x/8]&(0x80u>>(x%8))?255:0);
    fclose(f);
}
int main(void) {
    const char *items[]={"DISPLAY","CONTROLLER","SOUND","FPS"};
    screen_draw_menu(first,"SETTINGS / FAST GRAYSCALE","A CHOOSE / B BACK",items,4,0);
    screen_draw_menu(second,"IGNORED","IGNORED",items,4,0);
    assert(!memcmp(first,second,5000));
    for(unsigned y=0;y<40;++y)for(unsigned i=0;i<25;++i)assert(first[y*25+i]==255);
    for(unsigned y=160;y<200;++y)for(unsigned i=0;i<25;++i)assert(first[y*25+i]==255);
    save("settings",first);
    screen_draw_menu(second,"","",items,4,1);assert(memcmp(first,second,5000));
    const char *home[]={"RESUME GAME","GAMES","SETTINGS","POWER OFF"};
    screen_draw_menu(first,"HOME","GAME NAME",home,4,0);save("home",first);
    for(unsigned status=0;status<=CONTROLLER_USB_PC;++status) {
        screen_draw_menu(first,"HOME","",home,4,0);
        memcpy(second,first,5000);
        screen_draw_controller_status(first,(controller_status_t)status);
        assert(memcmp(first,second,5000));
        /* Badge changes only its top margin; menu selection remains untouched. */
        assert(!memcmp(first+30*25,second+30*25,5000-30*25));
        char name[32];snprintf(name,sizeof name,"controller-%u",status);save(name,first);
    }
    const char *adjust[]={"INCREASE","DECREASE"};
    screen_draw_value_menu(first,"BLACK PULSES: 8",adjust,1);save("value",first);
    return 0;
}
'''
run(CODE,modules=['screen_draw.c','gray_motion.c'])
print('PASS: actual menu pixels omit title/subtitle/footer, keep margins clear and selection visible')
