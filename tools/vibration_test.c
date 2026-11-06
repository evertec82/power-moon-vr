#include <assert.h>
#include <stdio.h>
#include "../runtime/vibration.h"
int main(void){VibrationMixer m={0};uint64_t t=1000000000;VibrationOutput o;
 vibration_publish(&m,0,NAN,-1,t);o=vibration_output(&m,t,true);assert(o.low==0&&o.high==0);
 vibration_publish(&m,0,2,.01f,t);o=vibration_output(&m,t,true);assert(o.low==1&&o.high==0&&o.hand[0]==1&&o.hand[1]==0);
 vibration_publish(&m,1,.1f,.3f,t+100000000);o=vibration_output(&m,t+200000000,true);assert(o.low==1&&o.high>0&&o.hand[1]>0);
 o=vibration_output(&m,t+250000000,true);assert(o.low<1&&o.hand[0]==0&&o.hand[1]>0);
 o=vibration_output(&m,t+350000000,true);assert(!o.low&&!o.high&&!o.hand[0]&&!o.hand[1]);
 o=vibration_output(&m,t-1,true);assert(!o.low&&!o.high);
 o=vibration_output(&m,t+100000000,false);assert(!o.low&&!o.high);
 assert(vibration_motor(INFINITY)==0&&vibration_motor(-1)==0&&vibration_motor(2)==1);
 puts("Vibration mixing: per-side TTL, band/hand mapping, gain/clamping, invalid values and disabled output pass");}