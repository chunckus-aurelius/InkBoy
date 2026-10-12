#!/usr/bin/env python3
"""Compare H's lookup conversion against the existing per-pixel renderer."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <string.h>
#include "gb_video.h"
static uint32_t seed=1;
static uint8_t random_byte(void){seed=seed*1664525u+1013904223u;return seed>>24;}
int main(void){
    gb_video_t a={0},b={0};uint8_t packed[GB_PACKED_BYTES],old[GB_PANEL_BYTES],fast[GB_PANEL_BYTES];
    for(unsigned i=0;i<GB_PANEL_BYTES;++i)old[i]=fast[i]=random_byte();
    // Every packed shade combination in every dither row phase.
    for(unsigned value=0;value<256;++value){
        memset(packed,value,sizeof(packed));
        assert(gb_video_convert(&a,packed,old)==gb_video_convert_fast(&b,packed,fast));
        assert(!memcmp(old,fast,sizeof(old)) && !memcmp(a.previous,b.previous,sizeof(a.previous)));
        assert(gb_video_convert_fast(&b,packed,fast)==0);
    }
    // Grayscale never creates a dither pattern, including its full-clean seed.
    gb_video_t gray={0}; uint8_t clean[GB_PANEL_BYTES], before[GB_PANEL_BYTES];
    for(unsigned value=0;value<256;++value){
        for(unsigned i=0;i<GB_PANEL_BYTES;++i)clean[i]=before[i]=random_byte();
        memset(packed,value,sizeof(packed));
        assert(gb_video_prepare_gray(&gray,packed,clean)==GB_HEIGHT);
        for(unsigned y=0;y<200;++y)for(unsigned x=0;x<200;++x){
            unsigned i=y*25+x/8, bit=128u>>(x%8);
            if(y>=28 && y<172 && x>=20 && x<180)assert(!(clean[i]&bit));
            else assert((clean[i]&bit)==(before[i]&bit));
        }
        assert(gb_video_prepare_gray(&gray,packed,clean)==0);
    }
    packed[3*GB_PACKED_ROW]^=1;
    assert(gb_video_prepare_gray(&gray,packed,clean)==1);
    // Mix unchanged rows, sparse edits, arbitrary pixels, resets and margins.
    for(unsigned frame=0;frame<100;++frame){
        for(unsigned y=0;y<GB_HEIGHT;++y)if(random_byte()%3)
            for(unsigned x=0;x<GB_PACKED_ROW;++x)packed[y*GB_PACKED_ROW+x]=random_byte();
        if(frame%7==0)a.valid=b.valid=false;
        assert(gb_video_convert(&a,packed,old)==gb_video_convert_fast(&b,packed,fast));
        assert(!memcmp(old,fast,sizeof(old)));
    }
}
'''
with tempfile.TemporaryDirectory(prefix='video-fast-') as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(root/'components/esp32-gameboy/src/gb_core'),str(p/'test.c'),
                    str(root/'components/esp32-gameboy/src/gb_core/gb_video.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: all 256 packed patterns and four dither phases, random dirty rows, unchanged frames, margins and reseeding')
