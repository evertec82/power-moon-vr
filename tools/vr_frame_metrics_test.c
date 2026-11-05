#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_frame_metrics.h"
int main(void) {
    VrFrameMetrics s={0};uint64_t t=1000000000;
    vr_metrics_record(&s,t);
    for(int i=0;i<99;i++)vr_metrics_record(&s,t+=14000000);
    vr_metrics_record(&s,t+=28000000);
    assert(s.count==100 && s.longest==28000000);
    assert(vr_metrics_percentile(&s,50)>13.9 && vr_metrics_percentile(&s,95)<14.3);
    assert(vr_metrics_percentile(&s,100)>27.9);
    vr_metrics_clear(&s);assert(!s.count && s.last==t);
    vr_metrics_record(&s,t+=2000000000);assert(s.count==1 && s.longest==2000000000);
    assert(vr_metrics_percentile(&s,99)==127.875);
    vr_metrics_record(&s,t-1);assert(s.count==1);
    puts("VR frame interval metrics passed");
}
