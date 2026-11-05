#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/vr_math.h"
#include "../runtime/nvn/vulkan/vr_pass.h"
static void near(float a,float b) { assert(fabsf(a-b)<1e-4f); }
static void point(float *o,const float *m,const float *v) {
    for(int r=0;r<4;r++) { o[r]=0; for(int c=0;c<4;c++) o[r]+=m[c*4+r]*v[c]; }
}
int main(void) {
    float p[16],inv[16],id[16],v[16],r[4];
    vr_projection(p,-.7f,.8f,.8f,-.7f,.1f,100);
    assert(vr_inverse(inv,p)); vr_multiply(id,p,inv);
    for(int i=0;i<16;i++) near(id[i],i%5==0?1:0);
    float n[4]={0,0,-.1f,1}; point(r,p,n); near(r[2]/r[3],0);
    n[2]=-100; point(r,p,n); near(r[2]/r[3],1);
    // Moving the right eye right shifts a centered object left; the left eye shifts it right.
    n[2]=-2;
    vr_pose_inverse(v,0,0,0,1,.032f,0,0); point(r,v,n); assert(r[0]<0);
    vr_pose_inverse(v,0,0,0,1,-.032f,0,0); point(r,v,n); assert(r[0]>0);
    // A +90 degree head yaw turns world-forward into eye-right.
    vr_pose_inverse(v,0,sqrtf(.5f),0,sqrtf(.5f),0,0,0); point(r,v,n); near(r[0],2);
    // With the renderer's negative-height viewport, world-up must map towards the image's top.
    vr_eye_projection(p,-.7f,.8f,.8f,-.7f,.1f,100);
    float up[4]={0,1,-2,1}; point(r,p,up);
    assert(r[1]/r[3]>0); assert(.5f-.5f*r[1]/r[3]<.5f);
    // Looking up pitches a previously centered stationary object down, not further up.
    vr_pose_inverse(v,sinf(.15f),0,0,cosf(.15f),0,0,0);
    float rotated[4]; point(rotated,v,n); point(r,p,rotated);
    assert(rotated[1]<0); assert(.5f-.5f*r[1]/r[3]>.5f);
    // Native camera distances must survive projection/unprojection in game units.
    vr_projection(p,-.574f,.574f,.34906585f,-.34906585f,25,500000);
    assert(vr_inverse(inv,p));
    float native_point[4]={0,0,-600,1},clip[4],restored[4];
    point(clip,p,native_point); point(restored,inv,clip);
    assert(fabsf(restored[2]/restored[3]+600)<.01f);
    // A 64 mm IPD at 100 game units/metre gives 3.2 units per eye, not an amplified shift.
    vr_pose_inverse(v,0,0,0,1,3.2f,0,0);
    point(rotated,v,restored);
    point(r,p,rotated);
    assert(fabsf(r[0]/r[3]+p[0]*3.2f/600)<1e-4f);
    float singular[16]={0}; assert(!vr_inverse(inv,singular));
    assert(vr_classify_pass(0,0,560,false,3,1,1600,900)==VR_PASS_SCREEN);
    assert(vr_classify_pass(8,16,560,false,3,1,1600,900)==VR_PASS_SCREEN);
    assert(vr_classify_pass(8,560,560,true,6,1,1600,900)==VR_PASS_SCREEN);
    assert(vr_classify_pass(8,560,560,false,6,1,1600,900)==VR_PASS_HUD);
    assert(vr_classify_pass(24,256,4800,false,96,1,720,405)==VR_PASS_WORLD);
    assert(vr_classify_pass(24,256,4800,false,96,1,2048,2048)==VR_PASS_SCREEN);
    assert(vr_classify_pass(0x18,256,64,false,4,1,1600,900)==VR_PASS_WORLD);
    assert(vr_classify_pass(0x702,0,64,false,3,0,1600,900)==VR_PASS_SCREEN);
    assert(vr_classify_pass(0x18,256,64,false,6,0,1600,900)==VR_PASS_SCREEN);
    assert(vr_classify_pass(0x702,0,64,false,96,0,1600,900)==VR_PASS_WORLD);
    assert(vr_depth_screen_pass(3,0x10,3,true));
    assert(!vr_depth_screen_pass(4,0x702,0xff,true));
    assert(!vr_depth_screen_pass(3,0x10,3,false));
    puts("VR matrix tests passed");
}
