#include "gray_motion.h"
#include <string.h>

void gray_motion_render(uint8_t *packed, unsigned x)
{
    if (x > GRAY_MOTION_MAX_X) x=GRAY_MOTION_MAX_X;
    for (unsigned bg=0; bg<4; ++bg) {
        memset(packed+bg*36*40, (3-bg)*85, 36*40);
        for (unsigned lane=0; lane<3; ++lane) {
            unsigned shade=3-gray_motion_patch(bg,lane);
            for (unsigned y=bg*36+lane*12+2; y<bg*36+lane*12+10; ++y)
                for (unsigned xx=x; xx<x+GRAY_MOTION_WIDTH; ++xx) {
                    unsigned byte=y*40+xx/4, shift=(xx%4)*2;
                    packed[byte]=(packed[byte] & ~(3u<<shift)) | (shade<<shift);
                }
        }
    }
}
