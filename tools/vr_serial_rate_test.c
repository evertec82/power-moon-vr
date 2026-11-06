#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_pacing.h"
int main(void){
 for(int hz=72;hz<=120;hz+=6){VrRateClock c={0};int64_t t=0,p=1000000000ll/hz;unsigned images=0;
  for(int game=0;game<600;game++){vr_rate_begin_source(&c,t,(int64_t)game*VR_GAME_PERIOD_NS);do{images++;if(!vr_rate_more(&c,t,p))break;t+=p;}while(1);t+=p;}
  assert(images>=(unsigned)(hz*10-1)&&images<=(unsigned)(hz*10+1));}
 VrRateClock c={0};vr_rate_begin_source(&c,1000000000,500000000);vr_rate_begin_source(&c,1050000001,550000001);
 assert(c.start==1050000001);assert(c.deadline==1066666668);
 vr_rate_begin_source(&c,3000000000,2500000000);assert(c.start==3000000000);
 puts("72-120 Hz cadence, skipped simulation steps and suspension reset checks passed");
}
