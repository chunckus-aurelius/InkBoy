#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { GRAY_ROUTE_LKL, GRAY_ROUTE_DKD, GRAY_ROUTE_DWD, GRAY_ROUTE_LWL,
       GRAY_ROUTE_DLD, GRAY_ROUTE_LDL, GRAY_ROUTES };
/* Byte-only preset extension. Gain 0 follows the normal retarget setting. */
typedef struct {
    uint8_t history_mode; /* 0 off, bit0 completed returns, bit1 interrupted returns */
    uint8_t weak_wl;      /* 0 normal, 1 nominal 11V, 2 nominal 8.8V */
    int8_t return_delta[GRAY_ROUTES];
    uint8_t reverse_percent[GRAY_ROUTES];
} gray_adaptive_t;
#define GRAY_ADAPTIVE_DEFAULT {3,0,{-1,0,0,0,0,0},{150,0,0,0,0,0}}
_Static_assert(sizeof(gray_adaptive_t)==14,"Stable v3 adaptive settings layout");
static inline bool gray_adaptive_valid(const gray_adaptive_t *c) {
    if (!c || c->history_mode>3 || c->weak_wl>2) return false;
    for(unsigned i=0;i<GRAY_ROUTES;++i) {
        unsigned g=c->reverse_percent[i];
        if(c->return_delta[i]<-3 || c->return_delta[i]>3 ||
           (g && (g<50 || g>200 || g%25))) return false;
    }
    return true;
}
/* Gray-target round trips are tracked only when a D>L>D or L>D>L control is non-neutral. */
static inline bool gray_adaptive_gray_routes(const gray_adaptive_t *c) {
    return c->return_delta[GRAY_ROUTE_DLD] || c->return_delta[GRAY_ROUTE_LDL] ||
           c->reverse_percent[GRAY_ROUTE_DLD] || c->reverse_percent[GRAY_ROUTE_LDL];
}
static inline int gray_adaptive_route(unsigned from,unsigned via,unsigned to) {
    if(from!=to) return -1;
    if(from==2 && via==0) return GRAY_ROUTE_LKL;
    if(from==1 && via==0) return GRAY_ROUTE_DKD;
    if(from==1 && via==3) return GRAY_ROUTE_DWD;
    if(from==2 && via==3) return GRAY_ROUTE_LWL;
    if(from==1 && via==2) return GRAY_ROUTE_DLD;
    if(from==2 && via==1) return GRAY_ROUTE_LDL;
    return -1;
}
