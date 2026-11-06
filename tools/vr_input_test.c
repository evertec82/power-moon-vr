#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_input_state.h"
int main(void){VrControllerRaw r={0};VrControllerState s=vr_controller_convert(&r);assert(s.buttons==0&&s.lx==0&&s.ry==0);
 r.primary[1]=true;s=vr_controller_convert(&r);assert(s.buttons==1);r.primary[1]=false;r.primary[0]=true;s=vr_controller_convert(&r);assert(s.buttons==2);
 r.primary[0]=r.primary[1]=r.secondary[0]=true;s=vr_controller_convert(&r);assert(s.buttons==7);
 r.secondary[1]=true;s=vr_controller_convert(&r);assert((s.buttons&(1ull<<10))&&!(s.buttons&12));
 memset(&r,0,sizeof r);r.trigger[0]=.9f;r.grip[0]=r.grip[1]=1;r.click[0]=r.click[1]=true;s=vr_controller_convert(&r);assert((s.buttons&(1ull<<11))&&!(s.buttons&192)&&((s.buttons&48)==48)&&(s.buttons&256));
 memset(&r,0,sizeof r);r.grip[0]=1;r.stick[1][0]=1;s=vr_controller_convert(&r);assert(s.buttons==(1ull<<14)&&s.rx==0);
 int32_t x,y;vr_controller_stick(.1f,.05f,&x,&y);assert(!x&&!y);vr_controller_stick(0,1,&x,&y);assert(!x&&y==32767);vr_controller_stick(NAN,0,&x,&y);assert(!x&&!y);
 x=24000;y=0;vr_merge_stick(&x,&y,0,1000);assert(x==24000&&y==0);vr_merge_stick(&x,&y,0,32767);assert(x==0&&y==32767);
 assert(vr_controller_fresh(100,200));assert(!vr_controller_fresh(100,99));assert(!vr_controller_fresh(1,250000002));assert(!vr_controller_fresh(0,10));
 puts("VR button/chord mapping, deadzone, gamepad coexistence and stale-input release checks passed");}
