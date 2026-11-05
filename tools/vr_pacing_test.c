#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_pacing.h"
int main(void) {
    const int rates[]={60,72,80,90,120,144,165,240};
    for(unsigned i=0;i<sizeof rates/sizeof *rates;i++) {
        int64_t period=1000000000ll/rates[i],time=0;
        unsigned frames=0; VrRateClock clock={0};
        for(int game=0;game<600;game++) {
            vr_rate_begin(&clock,time);
            do {
                float alpha=vr_rate_alpha(&clock,time);
                assert(alpha>=0 && alpha<=1);
                frames++;
                bool forecast=clock.samples+1<8 && vr_rate_next_within(&clock,time,period);
                bool again=vr_rate_more(&clock,time,period);
                assert(again==forecast);
                if(!again) break;
                time+=period;
            } while(1);
            time+=period;
        }
        assert(frames>=(unsigned)(rates[i]*10-1) && frames<=(unsigned)(rates[i]*10+1));
        printf("%d Hz: %u headset frames / 600 simulation frames\n",rates[i],frames);
        vr_rate_begin(&clock,time+1000000000ll);
        assert(clock.start==time+1000000000ll);
    }
    puts("VR pacing tests passed");
}
