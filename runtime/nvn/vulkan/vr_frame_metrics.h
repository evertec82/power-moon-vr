#pragma once
#include <stdint.h>
#include <string.h>
// Quarter-millisecond delivery intervals; the final bin includes long stalls.
// Keeping the last timestamp across reports preserves the boundary interval.
typedef struct { uint64_t last,longest; uint32_t bins[512],count; } VrFrameMetrics;
static void vr_metrics_observe(VrFrameMetrics *s,uint64_t delta) {
    uint64_t bin=delta/250000;
    s->bins[bin<512?bin:511]++;s->count++;
    if(delta>s->longest)s->longest=delta;
}
static void vr_metrics_record(VrFrameMetrics *s,uint64_t now) {
    if(s->last && now>s->last)vr_metrics_observe(s,now-s->last);
    s->last=now;
}
static double vr_metrics_percentile(const VrFrameMetrics *s,unsigned pct) {
    if(!s->count)return 0;
    uint64_t target=((uint64_t)s->count*pct+99)/100,sum=0;
    for(unsigned i=0;i<512;i++) {
        sum+=s->bins[i];
        if(sum>=target)return (i+.5)*.25;
    }
    return 127.875;
}
static void vr_metrics_clear(VrFrameMetrics *s) {
    uint64_t last=s->last;
    memset(s,0,sizeof *s);s->last=last;
}
