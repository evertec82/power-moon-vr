#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_simulation.h"
static double overload(unsigned base_ms,unsigned stereo_ms,uint64_t budget) {
    VrSimulationClock clock={0};
    uint64_t begin=1000000000,now=begin;
    unsigned games=0,images=0,consecutive=0;
    while(now-begin<UINT64_C(60000000000)) {
        now+=(uint64_t)base_ms*1000000;
        uint64_t due=vr_simulation_step(&clock,now);
        if(now<due) now=due;
        bool skip=vr_simulation_late_budget(now,due,budget) && consecutive<6;
        if(skip) consecutive++;
        else { consecutive=0; images++; now+=(uint64_t)stereo_ms*1000000; }
        games++;
    }
    double seconds=(now-begin)/1e9;
    printf("native %u ms, stereo %u ms: %.2f game updates/s, %.2f images/s\n",base_ms,stereo_ms,games/seconds,images/seconds);
    assert(games/seconds>59.8 && games/seconds<60.2);
    if(base_ms+stereo_ms>17) assert(images<games);
    return images/seconds;
}
int main(void) {
    VrSimulationClock c={0}; uint64_t t=1000000000;
    assert(vr_simulation_step(&c,t)==t);
    assert(vr_simulation_step(&c,t)==t+VR_SIM_PERIOD_NS);
    assert(vr_simulation_late(t+VR_SIM_PERIOD_NS+1,t));
    assert(!vr_simulation_late(t+1000000,t));
    assert(!vr_simulation_late(t-1000000,t));
    t+=1000000000;
    assert(vr_simulation_step(&c,t)==t);
    assert(vr_simulation_step(&c,1)==1); // clock reset
    overload(5,0,VR_SIM_PERIOD_NS);
    double tight=overload(5,20,2000000),bounded=overload(5,20,VR_SIM_PERIOD_NS);
    assert(bounded>tight*1.1);
    overload(5,40,VR_SIM_PERIOD_NS); overload(10,24,VR_SIM_PERIOD_NS);
    puts("Simulation catch-up tests passed");
}
