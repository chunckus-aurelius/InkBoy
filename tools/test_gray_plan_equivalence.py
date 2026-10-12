#!/usr/bin/env python3
"""Differential host test for the optimized grayscale planner.

Builds the frozen pre-optimization implementation under reference_* names and
compares it with main/gray_plan.c over deterministic stateful workloads.
"""

from pathlib import Path
import os
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''#include "gray_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void reference_gray_plan_seed(gray_plan_t *, const uint8_t *);
void reference_gray_plan_target(gray_plan_t *, const uint8_t *, const uint8_t *);
unsigned reference_gray_plan_masks(const gray_plan_t *, uint8_t *, uint8_t *);
void reference_gray_plan_complete(gray_plan_t *, const uint8_t *, const uint8_t *);
unsigned reference_gray_plan_prepare(gray_plan_t *, const uint8_t *, const uint8_t *, uint8_t *, uint8_t *);
unsigned reference_gray_plan_prepare_pair(gray_plan_t *, const uint8_t *, const uint8_t *,
    uint8_t *, uint8_t *, uint8_t *, uint8_t *, unsigned);
void reference_gray_plan_complete_pair(gray_plan_t *, const uint8_t *, const uint8_t *,
    const uint8_t *, const uint8_t *);

static gray_plan_t old_plan, new_plan;
static gray_history_t old_history[GRAY_PIXELS], new_history[GRAY_PIXELS];
static uint8_t binary[GRAY_PIXELS/8], game[5760];
static uint8_t b[GRAY_PIXELS/8], w[GRAY_PIXELS/8], fb[GRAY_PIXELS/8], fw[GRAY_PIXELS/8];
static uint8_t rb[GRAY_PIXELS/8], rw[GRAY_PIXELS/8], rfb[GRAY_PIXELS/8], rfw[GRAY_PIXELS/8];
static uint32_t rng=0x91e10da5u;

static uint32_t rand32(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
}
static void fail(unsigned step, const char *what, unsigned index) {
    fprintf(stderr, "Mismatch at step %u: %s[%u]\n", step, what, index);
    exit(1);
}
static void eqmem(unsigned step, const char *name, const void *a, const void *b, size_t n) {
    if (memcmp(a,b,n)) {
        const unsigned char *x=a, *y=b;
        size_t i=0; while(i<n && x[i]==y[i]) ++i;
        fail(step,name,(unsigned)i);
    }
}
static void compare(unsigned step) {
    eqmem(step,"pixel",old_plan.pixel,new_plan.pixel,sizeof(old_plan.pixel));
    eqmem(step,"active",old_plan.active,new_plan.active,sizeof(old_plan.active));
    eqmem(step,"previous_binary",old_plan.previous_binary,new_plan.previous_binary,sizeof(old_plan.previous_binary));
    eqmem(step,"previous_game",old_plan.previous_game,new_plan.previous_game,sizeof(old_plan.previous_game));
    eqmem(step,"weak_wl",old_plan.weak_wl,new_plan.weak_wl,sizeof(old_plan.weak_wl));
    eqmem(step,"history",old_history,new_history,sizeof(old_history));
    eqmem(step,"config",&old_plan.config,&new_plan.config,sizeof(old_plan.config));
    eqmem(step,"adaptive",&old_plan.adaptive,&new_plan.adaptive,sizeof(old_plan.adaptive));
    if(old_plan.pending!=new_plan.pending) fail(step,"pending",0);
    if(old_plan.had_game!=new_plan.had_game) fail(step,"had_game",0);
    if(old_plan.history_mode!=new_plan.history_mode) fail(step,"history_mode",0);
    if(old_plan.history_returns!=new_plan.history_returns) fail(step,"history_returns",0);
    if(old_plan.history_reversals!=new_plan.history_reversals) fail(step,"history_reversals",0);
    if(old_plan.adaptive_configured!=new_plan.adaptive_configured) fail(step,"adaptive_configured",0);
}
static void init_config(unsigned variant) {
    gray_config_t c=(gray_config_t)GRAY_CONFIG_DEFAULT;
    c.retarget_gain=variant%5; c.scan_policy=variant%2;
    for(unsigned s=0;s<4;s++) for(unsigned t=0;t<4;t++)
        if(s!=t) c.dose[s][t]=(uint8_t)(1+((variant*7+s*3+t*5)%GRAY_DOSE_MAX));
    old_plan.config=new_plan.config=c;
}
static void mutate_inputs(unsigned step) {
    /* Mostly sparse edits, with periodic broad changes to exercise dirty rows. */
    unsigned n=(step%31==0)?300:1+(rand32()%12);
    while(n--) {
        unsigned i=rand32()%sizeof(binary); binary[i]^=(uint8_t)(1u<<(rand32()%8));
    }
    n=(step%29==0)?250:rand32()%9;
    while(n--) { unsigned i=rand32()%sizeof(game); game[i]=(uint8_t)rand32(); }
}
static const uint8_t *choose_game(unsigned step) {
    switch(step%7) { case 0: case 1: case 5: return NULL; default: return game; }
}
static unsigned valid_hold(unsigned step) {
    /* Diagonal hold bits are mandatory by the public mask contract. */
    return GRAY_HOLD_DIAGONAL | ((step*0x1397u) & ~GRAY_HOLD_DIAGONAL & 0xffffu);
}
static void compare_masks(unsigned step, unsigned count, unsigned ref_count,
                          int pair, int follow) {
    if(count!=ref_count) fail(step,"active_mask_count",0);
    eqmem(step,"black mask",b,rb,sizeof(b)); eqmem(step,"white mask",w,rw,sizeof(w));
    if(pair && follow) {
        eqmem(step,"follow black mask",fb,rfb,sizeof(fb));
        eqmem(step,"follow white mask",fw,rfw,sizeof(fw));
    }
}
static void set_game_target(unsigned index, unsigned target) {
    unsigned x=index%200, y=index/200;
    if(x<20 || x>=180 || y<28 || y>=172) abort();
    unsigned j=(y-28)*160+x-20, shift=(j%4)*2;
    game[j/4]=(uint8_t)((game[j/4]&~(3u<<shift))|((3u-target)<<shift));
}
static void prime_route_state(unsigned index, unsigned origin, unsigned route_step) {
    memset(&old_plan,0,sizeof(old_plan)); memset(&new_plan,0,sizeof(new_plan));
    memset(old_history,0,sizeof(old_history)); memset(new_history,0,sizeof(new_history));
    memset(binary,0,sizeof(binary)); memset(game,0xff,sizeof(game));
    old_plan.history=old_history; new_plan.history=new_history;
    init_config(0);
    gray_adaptive_t a=(gray_adaptive_t)GRAY_ADAPTIVE_DEFAULT;
    a.history_mode=3; a.weak_wl=1;
    for(unsigned r=0;r<4;r++) { a.return_delta[r]=1; a.reverse_percent[r]=150; }
    old_plan.adaptive=new_plan.adaptive=a;
    old_plan.adaptive_configured=new_plan.adaptive_configured=true;
    old_plan.history_mode=new_plan.history_mode=3;
    reference_gray_plan_seed(&old_plan,binary); gray_plan_seed(&new_plan,binary);
    reference_gray_plan_target(&old_plan,binary,game); gray_plan_target(&new_plan,binary,game);
    /* Establish the origin through acknowledged delivery, keeping the cached
     * source image coherent with the model just as production does. */
    set_game_target(index,origin);
    reference_gray_plan_target(&old_plan,binary,game); gray_plan_target(&new_plan,binary,game);
    while(old_plan.pending) {
        unsigned rc=reference_gray_plan_masks(&old_plan,rb,rw);
        unsigned nc=gray_plan_masks(&new_plan,b,w);
        compare_masks(799,nc,rc,0,0);
        reference_gray_plan_complete(&old_plan,rb,rw);
        gray_plan_complete(&new_plan,b,w);
    }
    compare(800+route_step*20);
}
static void target_route(unsigned step) {
    reference_gray_plan_target(&old_plan,binary,game);
    gray_plan_target(&new_plan,binary,game);
    compare(step);
}
static void one_delivery(unsigned step) {
    unsigned rc=reference_gray_plan_prepare(&old_plan,binary,game,rb,rw);
    unsigned nc=gray_plan_prepare(&new_plan,binary,game,b,w);
    compare_masks(step,nc,rc,0,0);
    reference_gray_plan_complete(&old_plan,rb,rw);
    gray_plan_complete(&new_plan,b,w);
    compare(step);
}
static void history_route(unsigned route_step, unsigned origin, unsigned via,
                          int complete_first, unsigned expected_counter) {
    const unsigned index=28u*200u+20u, base=800u+route_step*20u;
    prime_route_state(index,origin,route_step);
    /* Force the cached source row dirty even when the first route shade is K,
     * which is already the packed game's initial value for this pixel. */
    binary[index/8] |= (uint8_t)(128u>>(index%8));
    set_game_target(index,via); target_route(base+1);
    if(complete_first) {
        unsigned guard=0;
        while(old_plan.pixel[index].remaining && guard++<GRAY_DOSE_MAX+1) one_delivery(base+2+guard);
        if(old_plan.pixel[index].remaining || new_plan.pixel[index].remaining) fail(base,"route did not settle",index);
        set_game_target(index,origin); target_route(base+12);
        if(expected_counter==1 && old_plan.history_returns==0) fail(base,"completed return counter",index);
        if(expected_counter==2 && old_plan.history_reversals==0) fail(base,"interrupted reversal counter",index);
    } else {
        one_delivery(base+2);
        set_game_target(index,origin); target_route(base+3);
        if(old_plan.history_reversals==0) fail(base,"interrupted reversal counter",index);
    }
}
static void exhaustive_completion(void) {
    enum { COMBINATIONS=193*4*GRAY_DOSE_MAX, PATTERNS=20 };
    /* The first four patterns exercise complete(); all sixteen patterns then
     * exercise complete_pair(), including conflicting and otherwise invalid
     * first/follow mask combinations. Every batch contains the full 40,000
     * pixel traversal, while its first COMBINATIONS pixels enumerate every
     * valid level/target/remaining tuple. */
    memset(binary,0,sizeof(binary));
    memset(&old_plan,0,sizeof(old_plan)); memset(&new_plan,0,sizeof(new_plan));
    memset(old_history,0,sizeof(old_history)); memset(new_history,0,sizeof(new_history));
    old_plan.history=old_history; new_plan.history=new_history; init_config(0);
    reference_gray_plan_seed(&old_plan,binary); gray_plan_seed(&new_plan,binary);
    unsigned pattern_id=0;
    for(unsigned paired=0;paired<=1;paired++) {
        unsigned limit=paired ? 16 : 4;
        for(unsigned mask_code=0;mask_code<limit;mask_code++,pattern_id++) {
            memset(b,0,sizeof(b)); memset(w,0,sizeof(w));
            memset(fb,0,sizeof(fb)); memset(fw,0,sizeof(fw));
            memset(rb,0,sizeof(rb)); memset(rw,0,sizeof(rw));
            memset(rfb,0,sizeof(rfb)); memset(rfw,0,sizeof(rfw));
            memset(old_plan.active,0xff,sizeof(old_plan.active));
            memset(new_plan.active,0xff,sizeof(new_plan.active));
            old_plan.pending=new_plan.pending=GRAY_PIXELS;
            for(unsigned i=0;i<GRAY_PIXELS;i++) {
                unsigned id=i%COMBINATIONS;
                unsigned remaining=id%GRAY_DOSE_MAX+1;
                unsigned target=(id/GRAY_DOSE_MAX)%4;
                unsigned level=id/(GRAY_DOSE_MAX*4);
                unsigned source=(level+target+remaining)&3;
                old_plan.pixel[i]=(gray_pixel_t){.level=(uint8_t)level,
                    .remaining=remaining,.target=target,.source=source};
                new_plan.pixel[i].packed=(uint16_t)(level|(remaining<<8)|(target<<12)|(source<<14));
                old_history[i]=new_history[i]=(gray_history_t){.origin=4,
                    .black_steps=(uint8_t)(250+(i&1)),.white_steps=(uint8_t)(254-(i&1))};
                unsigned bit=128u>>(i%8), byte=i/8;
                if(mask_code&1) { b[byte]|=bit; rb[byte]|=bit; }
                if(mask_code&2) { w[byte]|=bit; rw[byte]|=bit; }
                if(paired && (mask_code&4)) { fb[byte]|=bit; rfb[byte]|=bit; }
                if(paired && (mask_code&8)) { fw[byte]|=bit; rfw[byte]|=bit; }
            }
            if(paired) {
                reference_gray_plan_complete_pair(&old_plan,rb,rw,rfb,rfw);
                gray_plan_complete_pair(&new_plan,b,w,fb,fw);
            } else {
                reference_gray_plan_complete(&old_plan,rb,rw);
                gray_plan_complete(&new_plan,b,w);
            }
            compare(1000+pattern_id);
        }
    }
    printf("exhaustive completion: %u state tuples across %u mask patterns\n",
           COMBINATIONS,PATTERNS);
}

int main(void) {
    old_plan.history=old_history; new_plan.history=new_history;
    init_config(0);
    for(unsigned i=0;i<sizeof(binary);i++) binary[i]=(uint8_t)rand32();
    for(unsigned i=0;i<sizeof(game);i++) game[i]=(uint8_t)rand32();
    reference_gray_plan_seed(&old_plan,binary); gray_plan_seed(&new_plan,binary); compare(0);

    /* Exercise one settled L-K-L return and all four interrupted routes with
     * non-neutral adaptive settings, then verify that the relevant counters
     * actually advance in both implementations. */
    history_route(0,2,0,1,1);       /* completed L-K-L */
    history_route(1,2,0,0,2);       /* interrupted L-K-L */
    history_route(2,1,0,0,2);       /* interrupted D-K-D */
    history_route(3,1,3,0,2);       /* interrupted D-W-D */
    history_route(4,2,3,0,2);       /* interrupted L-W-L */

    exhaustive_completion();

    memset(&old_plan,0,sizeof(old_plan)); memset(&new_plan,0,sizeof(new_plan));
    memset(old_history,0,sizeof(old_history)); memset(new_history,0,sizeof(new_history));
    old_plan.history=old_history; new_plan.history=new_history;
    init_config(0);
    for(unsigned i=0;i<sizeof(binary);i++) binary[i]=(uint8_t)rand32();
    for(unsigned i=0;i<sizeof(game);i++) game[i]=(uint8_t)rand32();
    reference_gray_plan_seed(&old_plan,binary); gray_plan_seed(&new_plan,binary); compare(899);

    const unsigned steps=850;
    for(unsigned step=1;step<=steps;step++) {
        if(step%73==0) init_config(step/73);
        if(step%61==0) {
            gray_adaptive_t a=(gray_adaptive_t)GRAY_ADAPTIVE_DEFAULT;
            a.history_mode=(step/61)%4; a.weak_wl=(step/61)%3;
            for(unsigned i=0;i<4;i++) {
                a.return_delta[i]=(int8_t)((int)(rand32()%7)-3);
                a.reverse_percent[i]=(uint8_t)(50+25*(rand32()%7));
            }
            old_plan.adaptive=new_plan.adaptive=a;
            old_plan.adaptive_configured=new_plan.adaptive_configured=(step/61)%2;
            old_plan.history_mode=new_plan.history_mode=a.history_mode;
        }
        /* Detach history for short randomized windows, then reattach the same
         * storage to ensure optional provenance pointers remain equivalent. */
        if(step%151==110) old_plan.history=new_plan.history=NULL;
        if(step%151==121) old_plan.history=old_history, new_plan.history=new_history;
        mutate_inputs(step);
        const uint8_t *g=choose_game(step);
        unsigned action=rand32()%8;
        if(step%127==0 || action==0) {
            reference_gray_plan_seed(&old_plan,binary); gray_plan_seed(&new_plan,binary);
        } else if(action==1) {
            reference_gray_plan_target(&old_plan,binary,g); gray_plan_target(&new_plan,binary,g);
        } else if(action==2 || action==3) {
            unsigned rc=reference_gray_plan_prepare(&old_plan,binary,g,rb,rw);
            unsigned nc=gray_plan_prepare(&new_plan,binary,g,b,w);
            compare_masks(step,nc,rc,0,0);
            /* Some prepared waveforms are intentionally aborted. */
            if(action==2 || step%5==0) {
                reference_gray_plan_complete(&old_plan,rb,rw);
                gray_plan_complete(&new_plan,b,w);
            }
        } else if(action==4 || action==5) {
            unsigned hold=valid_hold(step);
            unsigned rc=reference_gray_plan_prepare_pair(&old_plan,binary,g,rb,rw,rfb,rfw,hold);
            unsigned nc=gray_plan_prepare_pair(&new_plan,binary,g,b,w,fb,fw,hold);
            compare_masks(step,nc,rc,1,1);
            if(action==4 || step%4==0) {
                reference_gray_plan_complete_pair(&old_plan,rb,rw,rfb,rfw);
                gray_plan_complete_pair(&new_plan,b,w,fb,fw);
            }
        } else {
            unsigned rc=reference_gray_plan_masks(&old_plan,rb,rw);
            unsigned nc=gray_plan_masks(&new_plan,b,w);
            compare_masks(step,nc,rc,0,0);
            if(action==7 && step%3==0) {
                reference_gray_plan_complete(&old_plan,rb,rw);
                gray_plan_complete(&new_plan,b,w);
            }
        }
        compare(step);
    }
    printf("equivalent across %u deterministic operations (seed 0x91e10da5)\n",steps);
    return 0;
}
'''


def main() -> int:
    cc = shlex.split(os.environ.get("CC", "cc"))
    with tempfile.TemporaryDirectory(prefix="gray-plan-equivalence-") as tmp:
        tmpdir = Path(tmp)
        harness = tmpdir / "equivalence.c"
        binary = tmpdir / "equivalence"
        harness.write_text(HARNESS)
        rename = [
            "gray_plan_seed=reference_gray_plan_seed",
            "gray_plan_target=reference_gray_plan_target",
            "gray_plan_masks=reference_gray_plan_masks",
            "gray_plan_complete=reference_gray_plan_complete",
            "gray_plan_prepare=reference_gray_plan_prepare",
            "gray_plan_prepare_pair=reference_gray_plan_prepare_pair",
            "gray_plan_complete_pair=reference_gray_plan_complete_pair",
        ]
        common = [
            "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(ROOT / "main"),
        ]
        ref_object = tmpdir / "reference.o"
        subprocess.run(cc + common + ["-c", str(ROOT / "tools/fixtures/gray_plan_pre_perf.c")]
                       + [f"-D{x}" for x in rename] + ["-o", str(ref_object)], check=True)
        subprocess.run(cc + common + [str(harness), str(ROOT / "main/gray_plan.c"),
                       str(ref_object), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, cwd=ROOT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
