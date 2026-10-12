#!/usr/bin/env python3
"""Stress completed-frame ownership with concurrent producer/consumer threads."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
code = r'''
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "latest_frame.h"
static latest_frame_t mailbox;
enum { N=30000, WORDS=GB_PACKED_BYTES/4 };
static void fill(uint32_t *p,unsigned n){for(unsigned i=0;i<WORDS;++i)p[i]=n^i;}
static void *produce(void *unused){
    (void)unused;uint32_t p[WORDS];
    for(unsigned n=1;n<=N;++n){fill(p,n);latest_frame_publish(&mailbox,(uint8_t*)p);if(n%17==0)sched_yield();}
    return NULL;
}
int main(void){
    uint32_t p[WORDS];fill(p,0);latest_frame_seed(&mailbox,(uint8_t*)p);
    latest_frame_copy(&mailbox,(uint8_t*)p);assert(p[0]==0);
    // Only the latest complete frame survives an arbitrarily long consumer pause.
    fill(p,1);latest_frame_publish(&mailbox,(uint8_t*)p);
    fill(p,2);latest_frame_publish(&mailbox,(uint8_t*)p);
    latest_frame_copy(&mailbox,(uint8_t*)p);assert(p[0]==2);
    fill(p,0);latest_frame_seed(&mailbox,(uint8_t*)p);
    pthread_t writer;assert(!pthread_create(&writer,NULL,produce,NULL));
    unsigned previous=0;
    do {
        latest_frame_copy(&mailbox,(uint8_t*)p);
        unsigned n=p[0];assert(n>=previous && n<=N);
        for(unsigned i=0;i<WORDS;++i)assert(p[i]==(n^i));
        previous=n;if(n%13==0)sched_yield();
    } while(previous<N);
    assert(!pthread_join(writer,NULL));
    // A new session/ROM replaces all three slots while the producer is stopped.
    fill(p,42);latest_frame_seed(&mailbox,(uint8_t*)p);
    latest_frame_copy(&mailbox,(uint8_t*)p);assert(p[0]==42);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='latest-frame-') as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-pthread','-I'+str(root/'main'),
                    '-I'+str(root/'components/esp32-gameboy/src/gb_core'),str(p/'test.c'),
                    str(root/'main/latest_frame.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,timeout=30)
print('PASS: concurrent complete frames, monotonic latest snapshots, no tearing, session reseed')
