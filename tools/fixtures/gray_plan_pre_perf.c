#include "gray_plan.h"
#include <string.h>
#include <stdbool.h>

void gray_plan_seed(gray_plan_t *p, const uint8_t *binary)
{
    if (!gray_config_valid(&p->config)) p->config = (gray_config_t)GRAY_CONFIG_DEFAULT;
    for (unsigned i=0; i<GRAY_PIXELS; ++i) {
        unsigned shade = binary[i/8] & (128u >> (i%8)) ? 3 : 0;
        p->pixel[i] = (gray_pixel_t){.level=shade*64, .target=shade, .source=shade};
        if (p->history) p->history[i]=(gray_history_t){.origin=4};
    }
    p->pending = 0;
    memset(p->active, 0, sizeof(p->active));
    memset(p->weak_wl, 0, sizeof(p->weak_wl));
    memcpy(p->previous_binary, binary, sizeof(p->previous_binary));
    p->had_game = false;
    p->history_returns=p->history_reversals=0;
}

static void retarget(gray_plan_t *plan, unsigned index, unsigned target)
{
    gray_pixel_t *p=&plan->pixel[index];
    const gray_config_t *config=&plan->config;
    gray_history_t *h=plan->history ? &plan->history[index] : NULL;
    if (target == p->target) return;
    /* Interpolate the calibrated doses between adjacent model shades. This
     * uses delivered progress, never the abandoned target as a settled source. */
    unsigned low = p->level / 64, fraction = p->level % 64;
    unsigned high = low < 3 ? low+1 : low;
    unsigned budget = (config->dose[low][target]*(64-fraction) + config->dose[high][target]*fraction + 63)/64;
    unsigned steps=h ? (p->target==0 ? h->black_steps : p->target==3 ? h->white_steps : 0) : 0;
    int route=h && steps ? gray_adaptive_route(h->origin,p->target,target) : -1;
    if (!plan->adaptive_configured && route!=GRAY_ROUTE_LKL) route=-1;
    int delta=route<0 ? 0 : plan->adaptive_configured ? plan->adaptive.return_delta[route] : -1;
    unsigned gain_override=route<0 ? 0 : plan->adaptive_configured ? plan->adaptive.reverse_percent[route] : 150;
    if (route>=0 && delta && (plan->history_mode&1) && !p->remaining &&
        p->level==p->target*64 && steps==config->dose[h->origin][p->target]) {
        int adjusted=(int)budget+delta;
        budget=adjusted<1 ? 1 : adjusted>(int)GRAY_DOSE_MAX ? GRAY_DOSE_MAX : (unsigned)adjusted;
        ++plan->history_returns;
    }
    if (p->remaining && budget) {
        unsigned gain=gray_retarget_percent(config);
        if (route>=0 && gain_override && (plan->history_mode&2)) {
            gain=gain_override; ++plan->history_reversals;
        }
        budget = (budget*gain+50)/100;
        if (!budget) budget = 1;
        if (budget > GRAY_DOSE_MAX) budget = GRAY_DOSE_MAX;
    }
    if (h) {
        h->origin=(target==0 || target==3) && !p->remaining && p->level%64==0 ? p->level/64 : 4;
        h->black_steps=h->white_steps=0;
    }
    unsigned bit=128u>>(index%8);
    /* A model source rounded up to W during an interrupted transition must not
     * receive weakened darkening. Require the previous white dose to finish. */
    if (plan->adaptive_configured && plan->adaptive.weak_wl && target==2 &&
        p->target==3 && !p->remaining && p->level==192)
        plan->weak_wl[index/8]|=bit;
    else plan->weak_wl[index/8]&=(uint8_t)~bit;
    /* Freeze the nearest estimated source for this transition. Do not change
     * pair policy as each pulse advances the intermediate level. */
    p->source = (p->level + 32u) / 64u;
    p->remaining = budget;
    p->target = target;
}

void gray_plan_target(gray_plan_t *p, const uint8_t *binary, const uint8_t *game)
{
    /* Dirty source rows avoid traversing all optical state on every scan.
     * Cached inputs are owned copies: the caller may reuse its snapshot buffer. */
    for (unsigned y=0; y<200; ++y) {
        bool viewport = y>=28 && y<172;
        bool changed = memcmp(p->previous_binary+y*25, binary+y*25, 25) != 0;
        if (viewport) {
            changed |= (game != NULL) != p->had_game;
            if (game && p->had_game)
                changed |= memcmp(p->previous_game+(y-28)*40, game+(y-28)*40, 40) != 0;
        }
        if (!changed) continue;
        for (unsigned x=0; x<200; ++x) {
            unsigned i=y*200+x, bit=128u >> (i%8);
            unsigned target = binary[i/8] & bit ? 3 : 0;
            if (game && viewport && x>=20 && x<180) {
                unsigned j=(y-28)*160+x-20;
                target = 3-((game[j/4] >> ((j%4)*2)) & 3);
            }
            gray_pixel_t *v=&p->pixel[i];
            if (target == v->target) continue;
            bool was_pending = v->remaining != 0;
            retarget(p, i, target);
            if (v->remaining) {
                p->active[i/8] |= bit;
                if (!was_pending) ++p->pending;
            } else {
                p->active[i/8] &= (uint8_t)~bit;
                if (was_pending) --p->pending;
            }
        }
        memcpy(p->previous_binary+y*25, binary+y*25, 25);
        if (game && viewport) memcpy(p->previous_game+(y-28)*40, game+(y-28)*40, 40);
    }
    p->had_game = game != NULL;
}

static unsigned prepare_masks(const gray_plan_t *p, uint8_t *black, uint8_t *white,
                               uint8_t *follow_black, uint8_t *follow_white, unsigned hold_mask)
{
    memset(black, 0, GRAY_PIXELS/8); memset(white, 0, GRAY_PIXELS/8);
    if (follow_black) {
        memset(follow_black,0,GRAY_PIXELS/8); memset(follow_white,0,GRAY_PIXELS/8);
    }
    unsigned active = 0;
    for (unsigned byte=0; byte<GRAY_PIXELS/8; ++byte) {
        unsigned bits=p->active[byte];
        while (bits) {
            unsigned shift=__builtin_ctz(bits), bit=1u<<shift;
            const gray_pixel_t *v=&p->pixel[byte*8+7-shift];
            uint8_t *mask = v->target*64 > v->level ? white : black;
            mask[byte] |= bit;
            if (follow_black && v->remaining > 1 &&
                !(hold_mask & (1u << (v->source*4u+v->target)))) {
                uint8_t *follow = v->target*64 > v->level ? follow_white : follow_black;
                follow[byte] |= bit;
            }
            bits &= ~bit;
            ++active;
        }
    }
    return active;
}

unsigned gray_plan_masks(const gray_plan_t *p, uint8_t *black, uint8_t *white)
{
    return prepare_masks(p,black,white,NULL,NULL,GRAY_HOLD_ALL);
}

unsigned gray_plan_prepare_pair(gray_plan_t *p, const uint8_t *binary, const uint8_t *game,
                                uint8_t *black, uint8_t *white, uint8_t *follow_black,
                                uint8_t *follow_white, unsigned hold_mask)
{
    gray_plan_target(p,binary,game);
    return prepare_masks(p,black,white,follow_black,follow_white,hold_mask);
}

unsigned gray_plan_prepare(gray_plan_t *p, const uint8_t *binary, const uint8_t *game,
                           uint8_t *black, uint8_t *white)
{
    gray_plan_target(p, binary, game);
    return gray_plan_masks(p, black, white);
}

static void complete_masks(gray_plan_t *p, const uint8_t *black, const uint8_t *white,
                            const uint8_t *follow_black, const uint8_t *follow_white)
{
    for (unsigned byte=0; byte<GRAY_PIXELS/8; ++byte) {
        unsigned fb=follow_black ? follow_black[byte] : 0;
        unsigned fw=follow_white ? follow_white[byte] : 0;
        unsigned bits=(black[byte] | white[byte] | fb | fw) & p->active[byte];
        while (bits) {
            unsigned shift=__builtin_ctz(bits), bit=1u<<shift;
            gray_pixel_t *v=&p->pixel[byte*8+7-shift];
            unsigned b=black[byte]&bit, w=white[byte]&bit;
            int distance=(int)v->target*64-v->level;
            unsigned steps=!(b && w) && ((distance>0 && w) || (distance<0 && b));
            if (!((fb & bit) && (fw & bit)) &&
                ((distance>0 && (fw & bit)) || (distance<0 && (fb & bit)))) ++steps;
            while (steps-- && v->remaining) {
                distance=(int)v->target*64-v->level;
                if (p->history && v->target==0 && distance<0) {
                    gray_history_t *h=&p->history[byte*8+7-shift];
                    if (h->black_steps<255) ++h->black_steps;
                }
                if (p->history && v->target==3 && distance>0) {
                    gray_history_t *h=&p->history[byte*8+7-shift];
                    if (h->white_steps<255) ++h->white_steps;
                }
                v->level = (int)v->level + distance/v->remaining;
                if (!--v->remaining) {
                    p->active[byte] &= (uint8_t)~bit;
                    --p->pending;
                }
            }
            bits &= ~bit;
        }
    }
}

void gray_plan_complete(gray_plan_t *p, const uint8_t *black, const uint8_t *white)
{
    complete_masks(p,black,white,NULL,NULL);
}

void gray_plan_complete_pair(gray_plan_t *p, const uint8_t *black, const uint8_t *white,
                             const uint8_t *follow_black, const uint8_t *follow_white)
{
    complete_masks(p,black,white,follow_black,follow_white);
}
