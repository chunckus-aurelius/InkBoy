#!/usr/bin/env python3
"""Check the fast drive bit planes against independent per-pixel budgets."""
from host_build import run

code = r'''
#include <assert.h>
#include <string.h>
#include "drive_counters.h"
static drive_counters_t d;
static uint8_t image[5000], black[5000], white[5000];
static uint8_t target[40000], budget[40000];
static unsigned rng=151691;
static unsigned random_word(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void seed(void){
    drive_counters_seed(&d,image);
    for(unsigned p=0;p<40000;++p){target[p]=(image[p/8]>>(7-p%8))&1;budget[p]=0;}
}
static void step(unsigned bp,unsigned wp,bool delivered){
    unsigned active=0;
    for(unsigned p=0;p<40000;++p){
        uint8_t next=(image[p/8]>>(7-p%8))&1;
        if(next!=target[p]){target[p]=next;budget[p]=next ? wp : bp;}
        active+=budget[p]!=0;
    }
    assert(drive_counters_prepare(&d,image,bp,wp,black,white)==active);
    assert(d.pending_pixels==active);
    for(unsigned p=0;p<40000;++p){
        unsigned bit=0x80u>>(p%8), byte=p/8;
        assert(((black[byte]&bit)!=0)==(budget[p]>0&&!target[p]));
        assert(((white[byte]&bit)!=0)==(budget[p]>0&&target[p]));
    }
    if(delivered){
        drive_counters_complete(&d,black,white);
        active=0;for(unsigned p=0;p<40000;++p){if(budget[p])--budget[p];active+=budget[p]!=0;}
        assert(d.pending_pixels==active);
    }
}
int main(void){
    assert(sizeof(d)==25004);
    for(unsigned symmetric=0;symmetric<2;++symmetric){
        for(unsigned i=0;i<5000;++i)image[i]=i%2 ? 255 : 0;
        seed();
        for(unsigned n=0;n<512;++n){
            if(n%13==0)for(unsigned i=1000;i<5000;++i)image[i]^=255;
            else for(unsigned k=0;k<90;++k){unsigned p=8000+random_word()%32000;image[p/8]^=0x80u>>(p%8);}
            unsigned bp=1+n%8,wp=symmetric ? bp : 1+(n/8)%8;
            step(bp,wp,n%17!=0); // Failure/skip does not consume.
            for(unsigned i=0;i<1000;++i)assert(!(black[i]|white[i]));
        }
        for(unsigned n=0;n<8;++n)step(8,8,true);
        assert(!d.pending_pixels);
        for(unsigned n=0;n<100;++n)step(8,8,true);
    }
    // Every asymmetric budget pair, all 40,000 pixels, including bit/word boundaries.
    for(unsigned bp=1;bp<=8;++bp)for(unsigned wp=1;wp<=8;++wp){
        memset(image,85,5000);seed();
        for(unsigned i=0;i<5000;++i)image[i]^=255;
        for(unsigned n=0;n<8;++n)step(bp,wp,true);
        assert(!d.pending_pixels);
    }
    image[0]^=128;image[4999]^=1;step(8,8,true);
    assert(d.pending_pixels==2);
    seed();assert(!d.pending_pixels);step(8,8,true);
    return 0;
}
'''
run(code, modules=['drive_counters.c'], optimize='-O2')
print('PASS: drive bit planes match 40,000 independent pixels; all 64 polarity budgets, reversals, failed delivery, untouched background and clean cancellation')
